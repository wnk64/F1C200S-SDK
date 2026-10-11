#!/usr/bin/env python3
"""Export a self-contained SDK delivery copy without copying build directories."""
import json
import os
from pathlib import Path
import shutil
import tempfile


def tree_inventory(root, sdk):
    result = {}
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root).as_posix()
        if path.is_symlink():
            if not sdk.contained(path, root) or not path.exists():
                raise RuntimeError("External/broken dependency symlink: " + str(path))
            result[relative] = {"symlink": os.readlink(str(path))}
        elif path.is_file():
            result[relative] = {"sha256": sdk.digest(path), "bytes": path.stat().st_size,
                                "mode": path.stat().st_mode & 0o777}
        elif not path.is_dir():
            raise RuntimeError("Non-file dependency: " + str(path))
    return result


def copy_tree(source, destination, sdk):
    if source.is_symlink() or not source.is_dir() or destination.exists() or destination.is_symlink():
        raise RuntimeError("Invalid dependency copy path")
    expected = tree_inventory(source, sdk)
    shutil.copytree(str(source), str(destination), symlinks=True)
    if tree_inventory(destination, sdk) != expected:
        raise RuntimeError("Dependency copy checksum/mode mismatch: " + str(source))
    return expected


def copy_baseline(source, destination, sdk):
    if source.is_symlink() or not source.is_file():
        raise RuntimeError("Missing/unsafe baseline material: " + str(source))
    expected = sdk.digest(source)
    if destination.exists() and sdk.digest(destination) != expected:
        raise RuntimeError("Baseline would overwrite a different tracked file: " + str(destination))
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(str(source), str(destination))
    if sdk.digest(destination) != expected:
        raise RuntimeError("Baseline checksum mismatch")
    return expected


def export_repository(source, destination, commits, head, sdk):
    sdk.run(["git", "clone", "--no-hardlinks", "--dissociate", "--no-checkout", source, destination])
    sdk.run(["git", "checkout", "--detach", head], destination)
    for commit in sorted(set(commits)):
        sdk.capture(["git", "cat-file", "-e", commit + "^{commit}"], destination)
        sdk.run(["git", "update-ref", "refs/sdk-lock/" + commit, commit], destination)
    alternates = destination / ".git/objects/info/alternates"
    if alternates.exists():
        raise RuntimeError("Export repository depends on external Git objects")
    sdk.run(["git", "remote", "remove", "origin"], destination)
    sdk.run(["git", "fsck", "--connectivity-only", "--no-dangling"], destination)


def remove_export_stub(root, destination, sdk):
    if not destination.exists():
        return
    if not sdk.contained(destination, root) or destination.is_symlink() or (destination / ".git").exists():
        raise RuntimeError("Unsafe generated component stub")
    relative = destination.relative_to(root).as_posix()
    tracked = set(sdk.capture(["git", "ls-files", "--", relative], root).splitlines())
    for path in destination.rglob("*"):
        if path.is_symlink() or (not path.is_dir() and path.relative_to(root).as_posix() not in tracked):
            raise RuntimeError("Unrecognized file in generated component stub")
    # Only discard fresh main-repository checkout files. Their Git objects stay
    # in the export; the independent component repository supplies build inputs.
    shutil.rmtree(str(destination))


def build_package(sdk):
    if sdk.doctor(release=False):
        raise RuntimeError("Package source/environment preflight failed")
    sdk.check_output(create=True)
    destination = sdk.OUT / "portable-sdk"
    if destination.exists() or destination.is_symlink():
        raise RuntimeError("Portable SDK already exists; refusing to overwrite it")
    head = sdk.capture(["git", "rev-parse", "HEAD"], sdk.ROOT)
    # Export only a committed recipe. Unrelated dirty development files stay in place.
    changed = sdk.capture(["git", "diff", "--name-only", "HEAD", "--", "sdk", "sdk-support"], sdk.ROOT)
    if changed:
        raise RuntimeError("Commit SDK recipe before packaging: " + changed)
    inputs = {"main_commit": head, "manifest_sha256": sdk.digest(sdk.ROOT / "sdk-support/manifest.json"),
              "repositories": {}, "baseline_material": {}, "board_deployed": False,
              "clean_build_verified": False, "hardware_accepted": False}
    with tempfile.TemporaryDirectory(prefix="portable-export-", dir=str(sdk.OUT)) as temporary:
        target = Path(temporary) / "sdk"
        project = sdk.MANIFEST["project_sources"]
        export_repository(sdk.ROOT, target, [head, project["commit"]], head, sdk)
        seen = set()
        for name, spec in sorted(sdk.MANIFEST["components"].items()):
            source, spec = sdk.validate_source(name)
            if spec["path"] in seen:
                inputs["repositories"][name] = {"path": spec["path"], "shared_repository": True,
                                                "commit": spec["commit"], "export_commit": spec.get("export_commit", spec["commit"])}
                continue
            seen.add(spec["path"])
            path = target / spec["path"]
            path.parent.mkdir(parents=True, exist_ok=True)
            remove_export_stub(target, path, sdk)
            export_repository(source, path, [spec["commit"], spec.get("export_commit", spec["commit"])], spec["commit"], sdk)
            inputs["repositories"][name] = {"path": spec["path"], "commit": spec["commit"],
                                            "export_commit": spec.get("export_commit", spec["commit"])}
            material = [".config"] if "config_sha256" in spec else []
            if "baseline_sha256" in spec:
                material.append(spec["artifact"])
            if name == "kernel":
                material += ["vmlinux", "System.map"]
            for relative in material:
                inputs["baseline_material"][spec["path"] + "/" + relative] = copy_baseline(source / relative, path / relative, sdk)
        protocol = sdk.MANIFEST["protocol_source"]
        archive = sdk.ROOT / protocol["archive"]
        if sdk.digest(archive) != protocol["sha256"]:
            raise RuntimeError("Protocol source archive mismatch")
        inputs["baseline_material"][protocol["archive"]] = copy_baseline(archive, target / protocol["archive"], sdk)
        downloads = sdk.ROOT / sdk.MANIFEST["components"]["rootfs"]["path"] / "dl"
        inputs["downloads"] = copy_tree(downloads, target / downloads.relative_to(sdk.ROOT), sdk)
        compiler = sdk.compiler_bin().parent
        toolchain = target / "TOOLCHAIN/linaro-7.2.1"
        toolchain.parent.mkdir(exist_ok=True)
        inputs["toolchain"] = copy_tree(compiler, toolchain, sdk)
        if sdk.capture([toolchain / "bin/arm-linux-gnueabi-gcc", "-dumpfullversion"]) != "7.2.1":
            raise RuntimeError("Relocated compiler failed version check")
        env = os.environ.copy()
        env.pop("KERNEL_TOOLCHAIN", None)
        env["LC_ALL"] = "C"
        env["PATH"] = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
        sdk.run([target / "sdk", "doctor"], target, env)
        if (target / ".sdk-output").exists():
            raise RuntimeError("Export unexpectedly contains generated build output")
        (target / "sdk-support/package-report.json").write_text(json.dumps(inputs, indent=2) + "\n")
        os.rename(str(target), str(destination))
    print("Portable source SDK exported (clean build/hardware acceptance pending): " + str(destination), flush=True)
