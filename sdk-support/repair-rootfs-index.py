#!/usr/bin/env python3
"""Migrate the reviewed package-index repair into generated Buildroot sources."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("f1sdk", str(HERE / "sdk.py"))
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)
ALLOWED = {"package/Config.in", "WORKTREE_CHANGELOG.md", "变更记录.md"}
CORE_PREFIXES = ("BR2_TOOLCHAIN_", "BR2_GCC_", "BR2_BINUTILS_", "BR2_ARM_", "BR2_KERNEL_HEADERS_")
CORE_KEYS = {"BR2_ARCH", "BR2_ENDIAN", "BR2_SOFT_FLOAT", "BR2_USE_MMU", "BR2_INSTALL_LIBSTDCPP", "BR2_USE_WCHAR", "BR2_ENABLE_LOCALE"}


def config_values(text):
    values = {}
    for line in text.splitlines():
        if line.startswith("BR2_") and "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
        elif line.startswith("# BR2_") and line.endswith(" is not set"):
            values[line[2:-11]] = "n"
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--commit", required=True)
    args = parser.parse_args()
    if not re.fullmatch("[0-9a-f]{40}", args.commit):
        parser.error("full source commit is required")
    with sdk.locked():
        sdk.check_output()
        original = sdk.ROOT / "ROOTFS/buildroot-2018.02.11"
        target = sdk.OUT / "work/ROOTFS/buildroot-2018.02.11"
        if target.is_symlink() or not sdk.contained(target, sdk.OUT):
            raise RuntimeError("Unsafe generated Buildroot path")
        stamp = target / ".sdk-source-commit"
        if stamp.is_symlink() or not stamp.is_file():
            raise RuntimeError("Missing generated source ownership stamp")
        old_commit = stamp.read_text().strip()
        if not re.fullmatch("[0-9a-f]{40}", old_commit):
            raise RuntimeError("Invalid previous source stamp")
        if sdk.capture(["git", "rev-parse", "HEAD"], original) != args.commit:
            raise RuntimeError("Source repository is not at the reviewed new commit")
        if sdk.capture(["git", "diff", "--name-only", "HEAD", "--", "package/Config.in"], original):
            raise RuntimeError("Package index has uncommitted changes")
        changed = set(sdk.capture(["git", "-c", "core.quotepath=false", "diff", "--name-only", old_commit, args.commit], original).splitlines())
        if not changed or changed - ALLOWED:
            raise RuntimeError("Source changes exceed the reviewed package-index/record repair: " + repr(sorted(changed)))
        index = target / "package/Config.in"
        old_index = index.read_bytes()
        old_config = (target / ".config").read_text()
        before = config_values(old_config)
        config = sdk.ROOT / "sdk-support/configs/rootfs.config"
        if sdk.digest(config) != "9b7f12245973997e63e4d18f2582e81de6291de771c03864c04a106ebe8c0455":
            raise RuntimeError("Original configuration input changed")
        try:
            index.write_bytes(subprocess.check_output(["git", "show", args.commit + ":package/Config.in"], cwd=str(original)))
            (target / ".config").write_bytes(config.read_bytes())
            sdk.set_config(target / ".config", {
                "BR2_ROOTFS_OVERLAY": '""', "BR2_ROOTFS_POST_BUILD_SCRIPT": '""',
                "BR2_ROOTFS_POST_FAKEROOT_SCRIPT": '""', "BR2_ROOTFS_POST_IMAGE_SCRIPT": '""',
                "BR2_PACKAGE_OVERRIDE_FILE": '"$(CONFIG_DIR)/local.mk"',
            })
            env = os.environ.copy()
            env["LC_ALL"] = "C"
            sdk.run(["make", "olddefconfig"], target, env)
            after = config_values((target / ".config").read_text())
            core = sorted(k for k in set(before) | set(after) if k.startswith(CORE_PREFIXES) or k in CORE_KEYS)
            differences = {k: [before.get(k, "n"), after.get(k, "n")] for k in core if before.get(k, "n") != after.get(k, "n")}
            if differences:
                raise RuntimeError("Toolchain configuration changed; clean toolchain required: " + json.dumps(differences))
            required = ["BR2_PACKAGE_BUSYBOX", "BR2_PACKAGE_SKELETON_INIT_COMMON", "BR2_PACKAGE_SKELETON_INIT_SYSV",
                        "BR2_PACKAGE_ALSA_LIB", "BR2_PACKAGE_DROPBEAR", "BR2_PACKAGE_BLUEZ5_UTILS", "BR2_PACKAGE_LIBCEDARC"]
            for key in required:
                if after.get(key) != "y":
                    raise RuntimeError("Required package was not restored: " + key)
            for relative in changed - {"package/Config.in"}:
                data = subprocess.check_output(["git", "show", args.commit + ":" + relative], cwd=str(original))
                (target / relative).write_bytes(data)
            stamp.write_text(args.commit + "\n")
            report = {"old_source_commit": old_commit, "new_source_commit": args.commit,
                      "changed_source_files": sorted(changed), "toolchain_config_unchanged": True,
                      "required_packages_restored": required,
                      "effective_config_sha256": sdk.digest(target / ".config")}
            (sdk.OUT / "rootfs-index-repair.json").write_text(json.dumps(report, indent=2) + "\n")
            print(json.dumps(report, indent=2))
        except BaseException:
            index.write_bytes(old_index)
            (target / ".config").write_text(old_config)
            raise


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print("Generated rootfs repair failed: {}".format(error), file=sys.stderr)
        sys.exit(1)
