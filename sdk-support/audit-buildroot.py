#!/usr/bin/env python3
"""Audit the preserved Buildroot tree against its exact upstream release."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("f1sdk", str(HERE / "sdk.py"))
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)
VERSION = "2018.02.11"
SHA256 = "4d9cb4b862877eb2493bd857e0e3d49427b00fdeecd03bf5c24bfc952b586e5a"
EXPECTED_MISSING = [
    "linux/0001-timeconst.pl-Eliminate-Perl-warning.patch.conditional",
    "linux/Config.ext.in", "linux/Config.in", "linux/linux-ext-aufs.mk",
    "linux/linux-ext-ev3dev-linux-drivers.mk", "linux/linux-ext-fbtft.mk",
    "linux/linux-ext-rtai.mk", "linux/linux-ext-xenomai.mk", "linux/linux.mk",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--restore-missing", action="store_true")
    args = parser.parse_args()
    original = sdk.ROOT / "ROOTFS" / ("buildroot-" + VERSION)
    archive = original / "dl" / ("buildroot-" + VERSION + ".tar.gz")
    if sdk.digest(archive) != SHA256:
        raise RuntimeError("Upstream release archive checksum mismatch")
    if "BR2_VERSION := " + VERSION not in (original / "Makefile").read_text():
        raise RuntimeError("Preserved tree is not the expected Buildroot version")
    with sdk.locked():
        sdk.check_output(create=True)
        destination = sdk.OUT / "upstream"
        destination.mkdir(exist_ok=True)
        upstream = destination / ("buildroot-" + VERSION)
        if not upstream.exists():
            with archive.open("rb") as stream:
                sdk.extract_archive(stream, destination)
        if "BR2_VERSION := " + VERSION not in (upstream / "Makefile").read_text():
            raise RuntimeError("Archive contains an unexpected version")
        missing, different, identical = [], [], 0
        for current_dir, directories, files in os.walk(str(upstream), followlinks=False):
            for name in files + [d for d in directories if (Path(current_dir) / d).is_symlink()]:
                source = Path(current_dir) / name
                relative = str(source.relative_to(upstream))
                target = original / relative
                if not target.exists() and not target.is_symlink():
                    missing.append(relative)
                elif source.is_symlink() or target.is_symlink():
                    if source.is_symlink() and target.is_symlink() and os.readlink(str(source)) == os.readlink(str(target)):
                        identical += 1
                    else:
                        different.append(relative)
                elif sdk.digest(source) != sdk.digest(target):
                    different.append(relative)
                else:
                    identical += 1
        report = {"upstream_version": VERSION, "archive_sha256": SHA256,
                  "identical_files": identical, "missing_files": sorted(missing),
                  "different_files_preserved": sorted(different)}
        report_path = sdk.OUT / "buildroot-source-audit.json"
        report_path.write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2))
        if args.restore_missing:
            if sorted(missing) != sorted(EXPECTED_MISSING):
                raise RuntimeError("Missing-file set differs from the reviewed nine-file repair")
            for relative in missing:
                target = original / relative
                if not sdk.contained(target, original) or target.exists() or target.is_symlink():
                    raise RuntimeError("Refusing to replace existing or escaping source: " + relative)
            for relative in missing:
                target = original / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(str(upstream / relative), str(target))
            print("Restored nine missing upstream source files; existing files were not overwritten")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError) as error:
        print("Buildroot audit failed: {}".format(error), file=sys.stderr)
        sys.exit(1)
