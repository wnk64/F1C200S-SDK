#!/usr/bin/env python3
"""Archive an independently rebuilt, cleaned SDK and its retained image report."""
import contextlib
import fcntl
import hashlib
import json
import os
from pathlib import Path
import tarfile

from package_sdk import tree_inventory


@contextlib.contextmanager
def source_lock(source):
    descriptor = os.open(str(source), os.O_RDONLY)
    try:
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise RuntimeError("Exported SDK is still in use")
        yield
    finally:
        os.close(descriptor)


def archive_tree(source, output, sdk):
    if source.is_symlink() or not source.is_dir() or (source / ".sdk-output").exists():
        raise RuntimeError("SDK must be a real, cleaned source directory")
    if output.exists() or output.is_symlink():
        raise RuntimeError("Refusing to overwrite an existing source archive")
    expected = tree_inventory(source, sdk)
    pending = output.with_name(output.name + ".pending")
    created = False
    try:
        with pending.open("xb") as stream:
            created = True
            with tarfile.open(fileobj=stream, mode="w:gz", compresslevel=1, format=tarfile.PAX_FORMAT) as archive:
                archive.add(str(source), arcname="F1c200S_SOFT", recursive=True)
        checked = set()
        with tarfile.open(str(pending), "r:gz") as archive:
            for member in archive:
                parts = Path(member.name).parts
                if not parts or parts[0] != "F1c200S_SOFT" or ".." in parts:
                    raise RuntimeError("Unsafe SDK archive member")
                if member.isdir():
                    continue
                relative = "/".join(parts[1:])
                if relative in checked or relative not in expected:
                    raise RuntimeError("Unexpected/duplicate SDK archive member: " + relative)
                checked.add(relative)
                spec = expected[relative]
                if "symlink" in spec:
                    if not member.issym() or member.linkname != spec["symlink"]:
                        raise RuntimeError("SDK archive link mismatch: " + relative)
                else:
                    if not member.isfile() or member.size != spec["bytes"] or member.mode != spec["mode"]:
                        raise RuntimeError("SDK archive file metadata mismatch: " + relative)
                    sha = hashlib.sha256()
                    with archive.extractfile(member) as stream:
                        for block in iter(lambda: stream.read(1024 * 1024), b""):
                            sha.update(block)
                    if sha.hexdigest() != spec["sha256"]:
                        raise RuntimeError("SDK archive checksum mismatch: " + relative)
        if checked != set(expected):
            raise RuntimeError("SDK archive omitted required files")
        os.rename(str(pending), str(output))
    finally:
        if created and pending.exists():
            pending.unlink()
    return {"archive": output.name, "sha256": sdk.digest(output), "bytes": output.stat().st_size,
            "files_verified": len(checked)}


def build_archive(sdk):
    sdk.check_output()
    source = sdk.OUT / "portable-sdk"
    if source.is_symlink() or not sdk.contained(source, sdk.OUT):
        raise RuntimeError("Unsafe exported SDK path")
    retained = sdk.runtime_destination(sdk.OUT, "images/portable-validation")
    with source_lock(source):
        package = json.loads((source / "sdk-support/package-report.json").read_text())
        full = json.loads((retained / "full-build.json").read_text())
        image = json.loads((retained / "sd-image.json").read_text())
        head = sdk.capture(["git", "rev-parse", "HEAD"], source)
        if head != package["main_commit"] or not full["build_complete"] or full["completed_steps"] != [name for name, unused in sdk.full_build_steps(full["jobs"])]:
            raise RuntimeError("Missing successful independent build for exported snapshot")
        if full["manifest_sha256"] != sdk.digest(source / "sdk-support/manifest.json") or full["recipe_sha256"] != sdk.digest(source / "sdk-support/sdk.py"):
            raise RuntimeError("Export recipe changed since independent build")
        if full["image_report_sha256"] != sdk.digest(retained / "sd-image.json"):
            raise RuntimeError("Retained image report mismatch")
        if image["sha256"] != full["image_sha256"] or sdk.digest(sdk.runtime_destination(retained, image["image"])) != image["sha256"]:
            raise RuntimeError("Retained independently built image mismatch")
        if (source / ".sdk-output").exists():
            raise RuntimeError("Run exported ./sdk clean before creating the clean delivery archive")
        for relative, expected in package["baseline_material"].items():
            if sdk.digest(source / relative) != expected:
                raise RuntimeError("Clean changed required baseline material: " + relative)
        for relative, spec in package["repositories"].items():
            repo = source / spec["path"]
            if sdk.capture(["git", "rev-parse", "HEAD"], repo) != spec["commit"]:
                raise RuntimeError("Clean changed component source: " + relative)
            for commit in set([spec["commit"], spec["export_commit"]]):
                if sdk.capture(["git", "rev-parse", "refs/sdk-lock/" + commit], repo) != commit:
                    raise RuntimeError("Required exported commit was not pinned")
        downloads = source / sdk.MANIFEST["components"]["rootfs"]["path"] / "dl"
        if tree_inventory(downloads, sdk) != package["downloads"] or tree_inventory(source / "TOOLCHAIN/linaro-7.2.1", sdk) != package["toolchain"]:
            raise RuntimeError("Clean changed download/toolchain dependencies")
        output = sdk.runtime_destination(sdk.OUT, "packages")
        output.mkdir(exist_ok=True)
        report = archive_tree(source, output / ("f1c200s-sdk-source-" + head[:12] + ".tar.gz"), sdk)
        report.update({"source_commit": head, "image_sha256": image["sha256"], "clean_build_verified": True,
                       "clean_verified": True, "hardware_accepted": False, "cold_boots": 0,
                       "scope": "independent source build/clean and archive verification; hardware acceptance pending"})
        (output / "build.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2), flush=True)


if __name__ == "__main__":
    import sdk
    with sdk.locked():
        build_archive(sdk)
