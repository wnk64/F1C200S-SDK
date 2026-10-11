#!/usr/bin/env python3
"""Source-locked F1C200S SDK. Generated files never replace active build trees."""
import argparse
import contextlib
import fcntl
import hashlib
import json
import os
from pathlib import Path
import posixpath
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / ".sdk-output"
MARKER = ".f1c200s-sdk-output"
TOKEN = "F1C200S generated SDK output v1\n"
MANIFEST = json.loads((ROOT / "sdk-support/manifest.json").read_text())
ROOTFS_REQUIRED_PACKAGES = [
    "BR2_PACKAGE_BUSYBOX", "BR2_PACKAGE_SKELETON_INIT_COMMON", "BR2_PACKAGE_SKELETON_INIT_SYSV",
    "BR2_PACKAGE_ALSA_LIB", "BR2_PACKAGE_DROPBEAR", "BR2_PACKAGE_BLUEZ5_UTILS", "BR2_PACKAGE_LIBCEDARC",
]


def digest(path):
    sha = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            sha.update(block)
    return sha.hexdigest()


def capture(args, cwd=None, env=None):
    return subprocess.check_output(list(map(str, args)), cwd=str(cwd) if cwd else None,
                                   env=env, universal_newlines=True).strip()


def run(args, cwd=None, env=None):
    args = list(map(str, args))
    print("+", " ".join(args), flush=True)
    subprocess.run(args, cwd=str(cwd) if cwd else None, env=env, check=True)


def resolved(path):
    return Path(os.path.realpath(str(path)))


def contained(path, parent):
    return resolved(path) != resolved(parent) and resolved(parent) in resolved(path).parents


def component(name):
    spec = MANIFEST["components"][name]
    path = ROOT / spec["path"]
    if not contained(path, ROOT) or path.is_symlink():
        raise RuntimeError("Source must be a real directory inside SDK: {}".format(path))
    return path, spec


def validate_source(name):
    path, spec = component(name)
    if capture(["git", "rev-parse", "--show-toplevel"], path) != str(path):
        raise RuntimeError("Missing complete component repository: {}".format(name))
    if capture(["git", "rev-parse", "HEAD"], path) != spec["commit"]:
        raise RuntimeError("Unreviewed source commit: {}; review manifest before updating".format(name))
    for required in spec.get("required_files", []):
        if not (path / required).is_file():
            raise RuntimeError("Missing required source file: {}/{}".format(name, required))
        capture(["git", "cat-file", "-e", "{}:{}".format(spec["commit"], required)], path)
    for relative, required in spec.get("required_text", {}).items():
        content = (path / relative).read_text()
        for text in required:
            if text not in content:
                raise RuntimeError("Required source index entry missing in {}: {}".format(relative, text))
    changed = set(capture(["git", "-c", "core.quotepath=false", "diff", "--name-only", "HEAD", "--"], path).splitlines())
    excluded = spec.get("excluded_submodules", {})
    for submodule, commit in excluded.items():
        entry = capture(["git", "ls-files", "--stage", "--", submodule], path)
        if entry != "160000 {} 0\t{}".format(commit, submodule):
            raise RuntimeError("Excluded legacy submodule reference changed: {}".format(submodule))
    if changed - set(excluded):
        raise RuntimeError("Tracked source differs from locked commit: {}".format(name))
    if "config_sha256" in spec and digest(path / ".config") != spec["config_sha256"]:
        raise RuntimeError("Configuration hash mismatch: {}".format(name))
    if "baseline_sha256" in spec:
        if digest(path / spec["artifact"]) != spec["baseline_sha256"]:
            raise RuntimeError("Active baseline artifact mismatch: {}".format(name))
    return path, spec


def compiler_bin():
    configured = os.environ.get("KERNEL_TOOLCHAIN")
    choices = [Path(configured)] if configured else [
        ROOT / "TOOLCHAIN/linaro-7.2.1/bin",
        Path("/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin"),
    ]
    for path in choices:
        gcc = path / "arm-linux-gnueabi-gcc"
        if gcc.is_file():
            version = capture([str(gcc), "-dumpfullversion"])
            if version != "7.2.1":
                raise RuntimeError("Expected Linaro GCC 7.2.1, got {}".format(version))
            return path.resolve()
    raise RuntimeError("Linaro 7.2.1 missing; set KERNEL_TOOLCHAIN to its bin directory")


def check_output(create=False):
    if OUT.is_symlink() or resolved(OUT) != ROOT / ".sdk-output":
        raise RuntimeError("Unsafe SDK output path")
    if OUT.exists():
        marker = OUT / MARKER
        if not OUT.is_dir() or marker.is_symlink() or not marker.is_file():
            raise RuntimeError("Refusing unmarked output directory")
        if marker.read_text() != TOKEN:
            raise RuntimeError("Invalid SDK output ownership marker")
    elif create:
        OUT.mkdir()
        (OUT / MARKER).write_text(TOKEN)


@contextlib.contextmanager
def locked():
    # Directory-inode locking also covers clean without recreating a lock file.
    descriptor = os.open(str(ROOT), os.O_RDONLY)
    try:
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise RuntimeError("Another SDK command is running") from error
        yield
    finally:
        os.close(descriptor)


def extract_archive(archive, target):
    with tarfile.open(fileobj=archive) as tree:
        for member in tree.getmembers():
            destination = target / member.name
            if not contained(destination, target):
                raise RuntimeError("Archive path escapes stage: {}".format(member.name))
            if member.issym() and not contained(destination.parent / member.linkname, target):
                raise RuntimeError("Archive symlink escapes stage: {}".format(member.name))
            if member.islnk() and not contained(target / member.linkname, target):
                raise RuntimeError("Archive hardlink escapes stage: {}".format(member.name))
            if not (member.isfile() or member.isdir() or member.issym() or member.islnk()):
                raise RuntimeError("Unsupported archive member: {}".format(member.name))
        tree.extractall(str(target))


def stage(name):
    source, spec = validate_source(name)
    commit = spec.get("export_commit", spec["commit"])
    capture(["git", "cat-file", "-e", commit + "^{commit}"], source)
    config = ROOT / "sdk-support/configs" / (name + ".config")
    if "config_sha256" in spec and digest(config) != spec["config_sha256"]:
        raise RuntimeError("SDK configuration copy mismatch: {}".format(name))
    check_output(create=True)
    target = OUT / spec.get("stage_path", "sources/" + name)
    if not contained(target, OUT):
        raise RuntimeError("Invalid stage path for {}".format(name))
    stamp = target / ".sdk-source-commit"
    if target.exists():
        if target.is_symlink() or stamp.is_symlink() or not stamp.is_file():
            raise RuntimeError("Staged source mismatch; run ./sdk clean: {}".format(target))
        previous = stamp.read_text().strip()
        if previous != commit:
            if not re.fullmatch("[0-9a-f]{40}", previous):
                raise RuntimeError("Invalid staged source stamp")
            changed = capture(["git", "-c", "core.quotepath=false", "diff", "--name-only", previous, commit], source).splitlines()
            if set(changed) - {"WORKTREE_CHANGELOG.md", "变更记录.md"}:
                raise RuntimeError("Staged code changed; clean this component before rebuilding")
            if changed:
                with tempfile.TemporaryFile() as archive:
                    subprocess.run(["git", "archive", commit] + changed, cwd=str(source), stdout=archive, check=True)
                    archive.seek(0)
                    extract_archive(archive, target)
            stamp.write_text(commit + "\n")
        return source, target, spec
    target.mkdir(parents=True)
    try:
        with tempfile.TemporaryFile() as archive:
            subprocess.run(["git", "archive", commit], cwd=str(source),
                           stdout=archive, check=True)
            archive.seek(0)
            extract_archive(archive, target)
        if "config_sha256" in spec:
            shutil.copy2(str(config), str(target / ".config"))
        stamp.write_text(commit + "\n")
    except BaseException:
        shutil.rmtree(str(target))
        raise
    return source, target, spec


def clean_component(name):
    check_output()
    if not OUT.exists():
        return
    source, spec = component(name)
    target = OUT / spec.get("stage_path", "sources/" + name)
    if not contained(target, OUT) or target.is_symlink():
        raise RuntimeError("Unsafe component output path")
    images = OUT / "images" / name
    if not contained(images, OUT) or images.is_symlink():
        raise RuntimeError("Unsafe component image path")
    if target.exists():
        marker = target / ".sdk-source-commit"
        if marker.is_symlink() or not marker.is_file():
            raise RuntimeError("Refusing component without source ownership stamp")
        commit = marker.read_text().strip()
        if not re.fullmatch("[0-9a-f]{40}", commit):
            raise RuntimeError("Invalid component source stamp")
        capture(["git", "cat-file", "-e", commit + "^{commit}"], source)
        shutil.rmtree(str(target))
    if images.exists():
        shutil.rmtree(str(images))


def config_values(text):
    values = {}
    for line in text.splitlines():
        if line.startswith("BR2_") and "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
        elif line.startswith("# BR2_") and line.endswith(" is not set"):
            values[line[2:-11]] = "n"
    return values


def set_config(path, values):
    lines = []
    for line in path.read_text().splitlines():
        key = line.split("=", 1)[0]
        if line.startswith("# ") and line.endswith(" is not set"):
            key = line[2:-11]
        if key not in values:
            lines.append(line)
    lines.extend(key + "=" + value for key, value in sorted(values.items()))
    path.write_text("\n".join(lines) + "\n")


def stage_project_sources():
    spec = MANIFEST["project_sources"]
    if capture(["git", "rev-parse", "--show-toplevel"], ROOT) != str(ROOT):
        raise RuntimeError("Missing project source repository")
    for relative in spec["paths"]:
        path = ROOT / relative
        if not contained(path, ROOT) or path.is_symlink() or not path.is_dir():
            raise RuntimeError("Invalid project source directory: " + relative)
        capture(["git", "cat-file", "-e", spec["commit"] + ":" + relative], ROOT)
    changed = set(capture(["git", "-c", "core.quotepath=false", "diff", "--name-only", spec["commit"], "--"] + spec["paths"], ROOT).splitlines())
    if changed - set(spec.get("unused_artifact_changes", [])):
        raise RuntimeError("Project source changed since locked snapshot: " + ", ".join(sorted(changed)))
    check_output(create=True)
    work = OUT / "work"
    if work.is_symlink() or not contained(work, OUT):
        raise RuntimeError("Unsafe application staging path")
    work.mkdir(exist_ok=True)
    stamps = OUT / "stamps"
    if stamps.is_symlink() or not contained(stamps, OUT):
        raise RuntimeError("Unsafe source stamp path")
    stamps.mkdir(exist_ok=True)
    stamp = stamps / "project-sources.json"
    if stamp.exists():
        if stamp.is_symlink() or json.loads(stamp.read_text()) != spec:
            raise RuntimeError("Project source lock changed; clean SDK output before restaging")
        return work
    for relative in spec["paths"]:
        if (work / relative).exists() or (work / relative).is_symlink():
            raise RuntimeError("Refusing to overwrite unrecognized staged project source: " + relative)
    with tempfile.TemporaryFile() as archive:
        subprocess.run(["git", "archive", spec["commit"]] + spec["paths"], cwd=str(ROOT),
                       stdout=archive, check=True)
        archive.seek(0)
        extract_archive(archive, work)
    stamp.write_text(json.dumps(spec, indent=2) + "\n")
    return work


def build_board(jobs):
    work = stage_project_sources()
    _, kernel, _ = stage("kernel")
    for required in ("vmlinux", "System.map", "scripts/dtc/dtc", "Module.symvers"):
        if not (kernel / required).is_file():
            raise RuntimeError("Build the isolated kernel first: " + required)
    env = os.environ.copy()
    env["PATH"] = str(compiler_bin()) + os.pathsep + env.get("PATH", "")
    env["LC_ALL"] = "C"
    base = ["make", "ARCH=arm", "CROSS_COMPILE=arm-linux-gnueabi-", "LOCALVERSION="]
    if capture(base + ["-s", "kernelrelease"], kernel, env) != MANIFEST["kernel_release"]:
        raise RuntimeError("Isolated kernel release mismatch")
    images = OUT / "images/board"
    images.mkdir(parents=True, exist_ok=True)
    source = work / MANIFEST["project_sources"]["dtb_source"]
    preprocessed = images / "board.preprocessed.dts"
    dtb = images / "suniv-f1c100s-licheepi-nano.dtb"
    network = work / "SCRCPT/network-and-audio-source"
    run(["arm-linux-gnueabi-gcc", "-E", "-nostdinc", "-undef", "-D__DTS__", "-x", "assembler-with-cpp",
         "-I", network / "newboard_rtl8723bs_20260930", "-I", network / "newboard_20260930",
         "-I", kernel / "arch/arm/boot/dts", "-I", kernel / "include", source, "-o", preprocessed], env=env)
    run([kernel / "scripts/dtc/dtc", "-I", "dts", "-O", "dtb", "-o", dtb, preprocessed], env=env)
    if digest(dtb) != MANIFEST["project_sources"]["dtb_sha256"]:
        raise RuntimeError("Rebuilt DTB is not identical to the current board DTB")
    modules = {}
    groups = [
        ("DRIVER/panel-and-board", ["tk032f8004_panel.ko"]),
        ("DRIVER/board-power-20261004", ["f1_board_power.ko"]),
        ("DRIVER/bluetooth-hid-20261010/src", ["hidp.ko", "uhid.ko"]),
    ]
    for relative, targets in groups:
        module_dir = work / relative
        run(base + ["-j{}".format(jobs), "M=" + str(module_dir), "modules"], kernel, env)
        for target in targets:
            path = module_dir / target
            if not path.is_file():
                raise RuntimeError("Missing required board module: " + str(path))
            vermagic = capture(["modinfo", "-F", "vermagic", path])
            if not vermagic.startswith(MANIFEST["kernel_release"] + " "):
                raise RuntimeError("Module release mismatch: " + str(path))
            shutil.copy2(str(path), str(images / path.name))
            modules[path.name] = {"sha256": digest(path), "vermagic": vermagic}
    report = {"source_commit": MANIFEST["project_sources"]["commit"], "dtb_sha256": digest(dtb),
              "modules": modules, "board_deployed": False, "cold_boots": 0}
    (images / "build.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


def build_app_ui(jobs):
    work = stage_project_sources()
    host = work / "ROOTFS/buildroot-2018.02.11/output/host"
    prefix = host / "bin/arm-buildroot-linux-gnueabi-"
    if not Path(str(prefix) + "gcc").is_file():
        raise RuntimeError("Build the isolated rootfs/toolchain first")
    env = os.environ.copy()
    env["PATH"] = str(host / "bin") + os.pathsep + env.get("PATH", "")
    env["LC_ALL"] = "C"
    common = ["make", "-j{}".format(jobs), "ROOT=" + str(work), "HOST=" + str(host),
              "CC=" + str(prefix) + "gcc", "CXX=" + str(prefix) + "g++",
              "AR=" + str(prefix) + "ar", "STRIP=" + str(prefix) + "strip"]
    groups = [
        ("LVGL/menu", ["f1-menu", "connections-helper", "cast-wait-touch", "f1-console-mode"]),
        ("LVGL/media_player", ["mplayer-ui", "media-browser"]),
        ("SOFTWARE/mic-recorder", ["mic-recorder"]),
        ("LVGL/sfc_native", ["sfc-native", "sfc-browser"]),
        ("LVGL/retro_native", ["retro-native", "retro-browser"]),
        ("LVGL/reader", ["reader", "reader-browser"]),
    ]
    results = {}
    images = OUT / "images/apps-ui"
    images.mkdir(parents=True, exist_ok=True)
    for directory, targets in groups:
        run(common + targets, work / directory, env)
        for target in targets:
            path = work / directory / target
            run([str(prefix) + "strip", path], env=env)
            shutil.copy2(str(path), str(images / target))
            results[target] = digest(path)
    report = {"source_commit": MANIFEST["project_sources"]["commit"], "sha256": results,
              "board_deployed": False, "scope": "UI/frontend subset, not all applications"}
    (images / "build.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


def build_rootfs(jobs, sources_only=False):
    original, target, spec = stage("rootfs")
    _, cedarc, _ = stage("libcedarc")
    # The legacy overlay is deliberately not part of this base-rootfs recipe.
    # Current board files are integrated separately, never collected from old payloads.
    set_config(target / ".config", {
        "BR2_ROOTFS_OVERLAY": '""',
        "BR2_ROOTFS_POST_BUILD_SCRIPT": '""',
        "BR2_ROOTFS_POST_FAKEROOT_SCRIPT": '""',
        "BR2_ROOTFS_POST_IMAGE_SCRIPT": '""',
        "BR2_PACKAGE_OVERRIDE_FILE": '"$(CONFIG_DIR)/local.mk"',
    })
    (target / "local.mk").write_text("LIBCEDARC_OVERRIDE_SRCDIR = {}\n".format(cedarc))
    downloads = original / "dl"
    if not contained(downloads, ROOT) or downloads.is_symlink():
        raise RuntimeError("Downloads must reside inside SDK")
    env = os.environ.copy()
    env["BR2_DL_DIR"] = str(downloads)
    env["LC_ALL"] = "C"
    run(["make", "olddefconfig"], target, env)
    selected = config_values((target / ".config").read_text())
    for key in ROOTFS_REQUIRED_PACKAGES:
        if selected.get(key) != "y":
            raise RuntimeError("Required rootfs package disappeared after configuration: " + key)
    run(["make", "-j{}".format(jobs), "source" if sources_only else "all"], target, env)
    if not sources_only:
        artifact = target / "output/images/rootfs.tar"
        if not artifact.is_file():
            raise RuntimeError("Buildroot did not produce the base rootfs archive")
        with tarfile.open(str(artifact)) as archive:
            members = {posixpath.normpath(member.name): member for member in archive.getmembers()}
        for required in ("bin/busybox", "sbin/init", "etc/inittab", "etc/passwd", "etc/shadow",
                         "usr/sbin/dropbear", "usr/bin/aplay", "usr/libexec/bluetooth/bluetoothd"):
            if required not in members:
                raise RuntimeError("Required base rootfs file missing: " + required)
        compiler = target / "output/host/bin/arm-buildroot-linux-gnueabi-gcc"
        images = OUT / "images/rootfs-base"
        images.mkdir(parents=True, exist_ok=True)
        report = {"source_commit": spec["commit"], "input_config_sha256": spec["config_sha256"],
                  "effective_config_sha256": digest(target / ".config"),
                  "rootfs_tar_sha256": digest(artifact), "rootfs_tar_bytes": artifact.stat().st_size,
                  "compiler": capture([compiler, "--version"]),
                  "scope": "base system only; runtime integration pending", "board_deployed": False}
        (images / "build.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2), flush=True)
    print("Base rootfs {} complete; runtime integration is still pending".format(
        "source acquisition" if sources_only else "build"), flush=True)


def build_boot(name, jobs):
    source, target, spec = stage(name)
    if name == "kernel":
        for required in (".config", "vmlinux", "System.map", "arch/arm/boot/zImage"):
            if not (source / required).is_file():
                raise RuntimeError("Incomplete protected kernel baseline: {}".format(required))
    env = os.environ.copy()
    env["PATH"] = str(compiler_bin()) + os.pathsep + env.get("PATH", "")
    env["LC_ALL"] = "C"
    env["KBUILD_BUILD_USER"] = "sdk"
    env["KBUILD_BUILD_HOST"] = "f1c200s"
    metadata = spec.get("build_metadata", {})
    localversion = "" if name == "kernel" else metadata.get("localversion", "")
    args = ["make", "ARCH=arm", "CROSS_COMPILE=arm-linux-gnueabi-", "LOCALVERSION=" + localversion]
    if metadata:
        env["SOURCE_DATE_EPOCH"] = str(metadata["source_date_epoch"])
        args.append("filechk_timestamp.h=printf '%s\\n' " + " ".join(shlex.quote(line) for line in metadata["timestamp_header"]))
    run(args + ["olddefconfig"], target, env)
    if name == "kernel":
        config = (target / ".config").read_text()
        if 'CONFIG_LOCALVERSION=""\n' not in config or "CONFIG_LOCALVERSION_AUTO=y" in config:
            raise RuntimeError("Kernel local version lock violated")
        release = capture(args + ["-s", "kernelrelease"], target, env)
        if release != MANIFEST["kernel_release"]:
            raise RuntimeError("Kernel release mismatch: {}".format(release))
    run(args + ["-j{}".format(jobs)], target, env)
    if name == "kernel":
        run(args + ["-j{}".format(jobs), "modules"], target, env)
    images = OUT / "images" / name
    images.mkdir(parents=True, exist_ok=True)
    artifacts = [spec["artifact"], ".config"]
    if name == "kernel":
        artifacts += ["vmlinux", "System.map", "Module.symvers"]
    results = {}
    for artifact in artifacts:
        path = target / artifact
        if not path.is_file():
            raise RuntimeError("Build did not produce {}".format(path))
        results[path.name] = digest(path)
    if spec.get("expected_output_sha256") and results[Path(spec["artifact"]).name] != spec["expected_output_sha256"]:
        raise RuntimeError("Rebuilt boot artifact differs from the verified live-board image")
    for artifact in artifacts:
        path = target / artifact
        shutil.copy2(str(path), str(images / path.name))
    report = {"component": name, "source_commit": spec.get("export_commit", spec["commit"]),
              "compiler": capture([str(compiler_bin() / "arm-linux-gnueabi-gcc"), "--version"]),
              "sha256": results, "board_deployed": False, "cold_boots": 0}
    (images / "build.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


def doctor(release=False):
    errors = []
    for name in MANIFEST["components"]:
        try:
            validate_source(name)
            config = ROOT / "sdk-support/configs" / (name + ".config")
            expected = MANIFEST["components"][name].get("config_sha256")
            if expected and digest(config) != expected:
                raise RuntimeError("SDK configuration copy mismatch: {}".format(name))
            print("OK source/config: {}".format(name))
        except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
            errors.append(str(error))
    try:
        print("OK kernel compiler: {}".format(compiler_bin()))
        check_output()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        errors.append(str(error))
    for command in ("make", "gcc", "bison", "flex", "bc", "openssl", "pkg-config"):
        if not shutil.which(command):
            errors.append("Missing host tool: {}".format(command))
    for blocker in MANIFEST["release_blockers"]:
        print("PENDING release: {}".format(blocker))
    if release and not MANIFEST["release_ready"]:
        errors.append("Full image is not release-ready; refusing an incomplete SDK image")
    for error in errors:
        print("ERROR: {}".format(error), file=sys.stderr)
    return 1 if errors else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command")
    inspect = sub.add_parser("doctor")
    inspect.add_argument("--release", action="store_true")
    build = sub.add_parser("build")
    build.add_argument("component", choices=["all", "kernel", "uboot", "rootfs-sources", "rootfs-base", "board", "apps-ui"], default="all", nargs="?")
    build.add_argument("-j", "--jobs", type=int, default=8)
    clean = sub.add_parser("clean")
    clean.add_argument("--component", choices=sorted(MANIFEST["components"]))
    args = parser.parse_args()
    if not args.command:
        parser.error("a command is required")
    with locked():
        if args.command == "doctor":
            return doctor(args.release)
        if args.command == "clean":
            if args.component:
                clean_component(args.component)
                print("Generated component output removed: " + args.component)
                return 0
            check_output()
            if OUT.exists():
                shutil.rmtree(str(OUT))
            print("SDK output removed; active source/config/toolchain/downloads preserved")
            return 0
        if args.jobs < 1 or args.jobs > 64:
            raise RuntimeError("jobs must be between 1 and 64")
        if args.component == "all":
            if doctor(release=True):
                return 1
            raise RuntimeError("Full SD image recipe has not been integrated yet")
        if args.component in ("rootfs-sources", "rootfs-base"):
            build_rootfs(args.jobs, sources_only=args.component == "rootfs-sources")
            return 0
        if args.component == "board":
            build_board(args.jobs)
            return 0
        if args.component == "apps-ui":
            build_app_ui(args.jobs)
            return 0
        build_boot(args.component, args.jobs)
        return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print("SDK failed: {}".format(error), file=sys.stderr)
        sys.exit(1)
