#!/usr/bin/env python3
"""Source-locked F1C200S SDK. Generated files never replace active build trees."""
import argparse
import contextlib
import copy
import fcntl
import hashlib
import io
import json
import os
from pathlib import Path
import posixpath
import re
import shlex
import shutil
import struct
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
    "BR2_PACKAGE_LIBDRM",
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
    metadata = set(spec.get("preserved_metadata_changes", []))
    if metadata - {"WORKTREE_CHANGELOG.md", "变更记录.md"}:
        raise RuntimeError("Only change records may be preserved as metadata")
    if changed - set(excluded) - set(spec.get("preserved_artifact_changes", [])) - metadata:
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
        for relative in spec.get("exclude_export_files", []):
            path = target / relative
            if not contained(path, target) or path.is_symlink():
                raise RuntimeError("Unsafe excluded generated artifact")
            if path.exists():
                if not path.is_file():
                    raise RuntimeError("Excluded artifact is not a file")
                path.unlink()
        if "config_sha256" in spec:
            shutil.copy2(str(config), str(target / ".config"))
        if spec.get("normalize_crlf"):
            for path in target.rglob("*"):
                if path.is_file() and not path.is_symlink() and path.suffix in (".am", ".ac", ".m4", ".sh", ".c", ".cpp", ".h", ".in", ".mk"):
                    content = path.read_bytes()
                    if b"\r\n" in content:
                        path.write_bytes(content.replace(b"\r\n", b"\n"))
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


def config_values(text, prefix="BR2_"):
    values = {}
    for line in text.splitlines():
        if line.startswith(prefix) and "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
        elif line.startswith("# " + prefix) and line.endswith(" is not set"):
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
        if stamp.is_symlink():
            raise RuntimeError("Project source lock changed; clean SDK output before restaging")
        previous = json.loads(stamp.read_text())
        if previous != spec:
            if previous["paths"] != spec["paths"] or previous["commit"] != spec.get("source_additions_from"):
                raise RuntimeError("Unreviewed project source migration")
            changed = set(capture(["git", "-c", "core.quotepath=false", "diff", "--name-only", previous["commit"], spec["commit"], "--"] + spec["paths"], ROOT).splitlines())
            added = set(capture(["git", "-c", "core.quotepath=false", "diff", "--diff-filter=A", "--name-only", previous["commit"], spec["commit"], "--"] + spec["paths"], ROOT).splitlines())
            if not changed or changed != added or changed != set(spec.get("source_additions", [])):
                raise RuntimeError("Project migration exceeds reviewed missing source additions")
            for relative in changed:
                destination = work / relative
                if not contained(destination, work) or destination.exists() or destination.is_symlink():
                    raise RuntimeError("Refusing to overwrite existing staged source: " + relative)
            with tempfile.TemporaryFile() as archive:
                subprocess.run(["git", "archive", spec["commit"]] + sorted(changed), cwd=str(ROOT), stdout=archive, check=True)
                archive.seek(0)
                extract_archive(archive, work)
            stamp.write_text(json.dumps(spec, indent=2) + "\n")
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


def build_kernel_runtime():
    _, kernel, spec = stage("kernel")
    for relative in (".config", "vmlinux", "System.map", "arch/arm/boot/zImage", "modules.order"):
        if not (kernel / relative).is_file():
            raise RuntimeError("Build complete isolated kernel first: " + relative)
    values = config_values((kernel / ".config").read_text(), prefix="CONFIG_")
    if values.get("CONFIG_LOCALVERSION") != '""' or values.get("CONFIG_LOCALVERSION_AUTO") == "y":
        raise RuntimeError("Kernel localversion configuration mismatch")
    env = os.environ.copy()
    env["PATH"] = str(compiler_bin()) + os.pathsep + env.get("PATH", "")
    base = ["make", "ARCH=arm", "CROSS_COMPILE=arm-linux-gnueabi-", "LOCALVERSION="]
    if capture(base + ["-s", "kernelrelease"], kernel, env) != MANIFEST["kernel_release"]:
        raise RuntimeError("Kernel release mismatch")
    images = OUT / "images/kernel-runtime"
    modules = {}
    superseded = ["drivers/bluetooth/hci_uart.ko"]
    for relative in (kernel / "modules.order").read_text().splitlines():
        path = kernel / relative
        if not contained(path, kernel) or path.is_symlink() or not path.is_file() or path.suffix != ".ko":
            raise RuntimeError("Missing/unsafe configured kernel module: " + relative)
        if relative in superseded:
            continue
        vermagic = capture(["modinfo", "-F", "vermagic", path])
        if not vermagic.startswith(MANIFEST["kernel_release"] + " "):
            raise RuntimeError("Configured kernel module release mismatch: " + relative)
        destination = images / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(str(path), str(destination))
        modules[relative] = {"sha256": digest(destination), "vermagic": vermagic}
    if not modules or "drivers/staging/rtl8723bs/r8723bs.ko" not in modules or "drivers/bluetooth/btrtl.ko" not in modules:
        raise RuntimeError("Required normal WLAN/Bluetooth module missing")
    external = json.loads((OUT / "images/wireless-modules/build.json").read_text())
    if not external["modules"]["hci_uart.ko"]["vermagic"].startswith(MANIFEST["kernel_release"] + " "):
        raise RuntimeError("Replacement H5 UART module missing/mismatched")
    report = {"source_commit": spec["commit"], "config_sha256": digest(kernel / ".config"),
              "zimage_sha256": digest(kernel / "arch/arm/boot/zImage"), "kernel_release": MANIFEST["kernel_release"],
              "modules": modules, "superseded_by_external_h5": superseded, "board_deployed": False}
    (images / "build.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Exported {} matching kernel modules".format(len(modules)), flush=True)


def build_wireless_modules(jobs):
    work = stage_project_sources()
    _, kernel, _ = stage("kernel")
    for required in (".config", "vmlinux", "System.map", "arch/arm/boot/zImage", "Module.symvers"):
        if not (kernel / required).is_file():
            raise RuntimeError("Build the isolated kernel first: " + required)
    values = config_values((kernel / ".config").read_text(), prefix="CONFIG_")
    if values.get("CONFIG_LOCALVERSION") != '""' or values.get("CONFIG_LOCALVERSION_AUTO") == "y":
        raise RuntimeError("Kernel localversion must remain empty and automatic suffixes disabled")
    env = os.environ.copy()
    env["PATH"] = str(compiler_bin()) + os.pathsep + env.get("PATH", "")
    env["LC_ALL"] = "C"
    base = ["make", "ARCH=arm", "CROSS_COMPILE=arm-linux-gnueabi-", "LOCALVERSION="]
    if capture(base + ["-s", "kernelrelease"], kernel, env) != MANIFEST["kernel_release"]:
        raise RuntimeError("Isolated kernel release mismatch")
    _, aic, _ = stage("aic-rxguard")
    _, peripherals, _ = stage("aic-peripherals")
    groups = [
        (aic / "drivers/aic8800", ["aic_load_fw/aic_load_fw.ko", "aic8800_fdrv/aic8800_fdrv.ko"],
         ["CONFIG_PREALLOC_RX_SKB=y", "CONFIG_F1C200S_PREALLOC_RX_SMALL=y"]),
        (peripherals / "drivers/aic8800/peripherals/ip5209_20260930", ["ip5209_power.ko"], []),
        (work / "SCRCPT/network-and-audio-source/newboard_20260930/bluetooth", ["hci_uart.ko"], []),
    ]
    images = OUT / "images/wireless-modules"
    images.mkdir(parents=True, exist_ok=True)
    modules = {}
    for directory, targets, flags in groups:
        run(base + ["-j{}".format(jobs), "M=" + str(directory)] + flags + ["modules"], kernel, env)
        for target in targets:
            path = directory / target
            if not path.is_file():
                raise RuntimeError("Missing required module: " + str(path))
            vermagic = capture(["modinfo", "-F", "vermagic", path])
            if not vermagic.startswith(MANIFEST["kernel_release"] + " "):
                raise RuntimeError("Module release mismatch: " + str(path))
            shutil.copy2(str(path), str(images / path.name))
            modules[path.name] = {"sha256": digest(path), "vermagic": vermagic}
    report = {"source_components": {name: MANIFEST["components"][name].get("export_commit", MANIFEST["components"][name]["commit"])
                                    for name in ("kernel", "aic-rxguard", "aic-peripherals")},
              "project_commit": MANIFEST["project_sources"]["commit"], "modules": modules,
              "kernel_release": MANIFEST["kernel_release"], "board_deployed": False, "cold_boots": 0}
    (images / "build.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


def relocate_cast_touch(source):
    old = '"/root/aic_miracast/candidates/audio_reopen_20261002/sink-audio-reopen"'
    new = '"/opt/f1/apps/cast/miracast_sink_dump.lowest"'
    content = source.read_text()
    if content.count(old) == 1 and new not in content:
        source.write_text(content.replace(old, new))
    elif content.count(new) != 1 or old in content:
        raise RuntimeError("Unrecognized cast touch sink identity")


def prepare_media_touch(source):
    support = ROOT / "sdk-support/runtime-media"
    candidate = support / "media_display.c"
    baseline = "bb154371198f10a50bfcc460b7c9c59cb7cccd087ff73cf57fe6c79851d0d6c1"
    if digest(source) not in (baseline, digest(candidate)):
        raise RuntimeError("Unrecognized media display source; refusing replacement")
    for name in ("media_display.c", "media_touch.h"):
        shutil.copyfile(str(support / name), str(source.with_name(name)))


def build_app_ui(jobs):
    work = stage_project_sources()
    relocate_cast_touch(work / "LVGL/menu/cast-wait-touch.c")
    prepare_media_touch(work / "LVGL/media_player/media_display.c")
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
              "cast_touch_source_sha256": digest(work / "LVGL/menu/cast-wait-touch.c"),
              "board_deployed": False, "scope": "UI/frontend subset, not all applications"}
    (images / "build.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


def app_environment():
    work = stage_project_sources()
    host = work / "ROOTFS/buildroot-2018.02.11/output/host"
    prefix = str(host / "bin/arm-buildroot-linux-gnueabi-")
    if not Path(prefix + "gcc").is_file():
        raise RuntimeError("Build the isolated rootfs/toolchain first")
    env = os.environ.copy()
    env["PATH"] = str(host / "bin") + os.pathsep + env.get("PATH", "")
    env["LC_ALL"] = "C"
    return work, host, prefix, env


def publish_apps(group, files, prefix):
    images = OUT / "images" / group
    images.mkdir(parents=True, exist_ok=True)
    results = {}
    for name, path in sorted(files.items()):
        path = Path(path)
        if not path.is_file() or not contained(path, OUT):
            raise RuntimeError("Missing or invalid application artifact: " + str(path))
        header = capture([prefix + "readelf", "-h", path])
        if "Machine:" not in header or "ARM" not in header:
            raise RuntimeError("Application is not an ARM ELF: " + str(path))
        dependencies = capture([prefix + "readelf", "-d", path])
        if "SDL" in dependencies:
            raise RuntimeError("Unexpected SDL dependency: " + str(path))
        run([prefix + "strip", path])
        shutil.copy2(str(path), str(images / name))
        results[name] = {"sha256": digest(path), "bytes": path.stat().st_size,
                         "dynamic_dependencies": dependencies}
    report = {"source_commit": MANIFEST["project_sources"]["commit"],
              "source_components": {name: spec.get("export_commit", spec["commit"]) for name, spec in MANIFEST["components"].items()},
              "artifacts": results, "board_deployed": False}
    (images / "build.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


def relocate_ons_baseline(directory):
    baseline = directory / "source-baseline.sha256"
    locked = baseline.read_text(encoding="ascii").splitlines()
    lines = []
    for line in locked:
        expected, filename = line.split(None, 1)
        candidate = directory / "vendor/ONScripter-GBK" / Path(filename).name
        if not contained(candidate, directory / "vendor/ONScripter-GBK") or candidate.is_symlink():
            raise RuntimeError("Unsafe ONS baseline source")
        if digest(candidate) != expected:
            raise RuntimeError("Vendored ONS baseline differs: " + candidate.name)
        lines.append(expected + "  vendor/ONScripter-GBK/" + candidate.name)
    baseline.write_text("\n".join(lines) + "\n", encoding="ascii")


def build_ons(jobs):
    work, host, prefix, env = app_environment()
    directory = work / "LVGL/ons_native"
    relocate_ons_baseline(directory)
    ext = directory / "build/extlib"
    ext.mkdir(parents=True, exist_ok=True)
    lib = ext / "lib"
    include = ext / "include"
    lib.mkdir(exist_ok=True)
    include.mkdir(exist_ok=True)
    common = ["make", "-j{}".format(jobs), "CC=" + prefix + "gcc", "CXX=" + prefix + "g++",
              "AR=" + prefix + "ar", "RANLIB=" + prefix + "ranlib"]
    bzip = directory / "vendor/extlib-src/bzip2-1.0.8"
    run(common + ["CFLAGS=-O2 -ffunction-sections -fdata-sections -D_FILE_OFFSET_BITS=64", "libbz2.a"], bzip, env)
    shutil.copy2(str(bzip / "libbz2.a"), str(lib / "libbz2.a"))
    shutil.copy2(str(bzip / "bzlib.h"), str(include / "bzlib.h"))
    for name in ("libogg-1.3.5", "libvorbis-1.3.7"):
        source = directory / "vendor/extlib-src" / name
        build = directory / "build" / name
        build.mkdir(exist_ok=True)
        library_env = env.copy()
        library_env.update({"CC": prefix + "gcc", "AR": prefix + "ar", "RANLIB": prefix + "ranlib",
                            "CFLAGS": "-O2 -ffunction-sections -fdata-sections", "CPPFLAGS": "-I" + str(include),
                            "LDFLAGS": "-L" + str(lib), "PKG_CONFIG_LIBDIR": str(lib / "pkgconfig")})
        if not (build / "Makefile").is_file():
            run([source / "configure", "--host=arm-buildroot-linux-gnueabi", "--disable-shared", "--enable-static",
                 "--prefix=" + str(ext)], build, library_env)
        run(["make", "-j{}".format(jobs)], build, library_env)
        run(["make", "install"], build, library_env)
    variables = ["ROOT=" + str(work), "HOST=" + str(host), "ENGINE=" + str(directory / "vendor/ONScripter-GBK"),
                 "EXT=" + str(ext)]
    run(common + variables + ["-f", "Makefile.tremor", "ons-lvgl-tremor"], directory, env)
    run(common + variables + ["-f", "Makefile.browser", "ons-rom-browser"], directory, env)
    publish_apps("ons", {"ons-lvgl-tremor": directory / "ons-lvgl-tremor", "ons-rom-browser": directory / "ons-rom-browser"}, prefix)


def build_retro_cores(jobs):
    work, host, prefix, env = app_environment()
    directory = work / "LVGL/retro_native"
    env["SOURCE_DATE_EPOCH"] = "1791590400"
    common = ["make", "-j{}".format(jobs), "platform=miyoo", "CC=" + prefix + "gcc",
              "CXX=" + prefix + "g++", "AR=" + prefix + "ar"]
    gpsp = directory / "vendor/gpsp"
    pcsx = directory / "vendor/pcsx"
    pico = directory / "vendor/pico"
    run(common + ["GIT_VERSION=5819380c", "SHARED=-shared -Wl,--version-script=link.T", "MMAP_JIT_CACHE=1"], gpsp, env)
    run(common + ["-f", "Makefile.libretro", "GIT_VERSION=acfca1d", "WANT_ZLIB=0", "HAVE_CHD=0",
                  "HAVE_PHYSICAL_CDROM=0", "USE_ASYNC_CDROM=0", "USE_ASYNC_GPU=0", "USE_ASYNC_SPU=0", "NDRC_THREAD=0"], pcsx, env)
    cyclone = pico / "cpu/cyclone"
    generator = directory / "build/cyclone-gen"
    generator.parent.mkdir(exist_ok=True)
    run(["g++", "-O2", "-DHAVE_ARMv6=0", '-DCONFIG_FILE="../cyclone_config.h"'] +
        sorted(cyclone.glob("*.cpp")) + [cyclone / "Disa/Disa.c", "-o", generator], env=env)
    run([generator], cyclone, env)
    run(common + ["-f", "Makefile.libretro", "GIT_REVISION=-26ecb2b", "SHARED=-shared", "LIBM=-lm", "use_libchdr=0"], pico, env)
    publish_apps("retro-cores", {"gba.so": gpsp / "gpsp_libretro.so", "ps1.so": pcsx / "pcsx_rearmed_libretro.so",
                                "md.so": pico / "picodrive_libretro.so"}, prefix)


def relocate_lan_backend(source):
    old = 'static const char *backend="/root/roms/candidates/mplayer_geometry_20261005/mplayer-native-geometry";'
    new = 'static const char *backend="/opt/f1/apps/media/mplayer-native";'
    content = source.read_text()
    if content.count(old) == 1 and new not in content:
        source.write_text(content.replace(old, new))
    elif content.count(new) != 1 or old in content:
        raise RuntimeError("Unrecognized DLNA backend source; refusing speculative relocation")


def prepare_lan_runtime(backend_source):
    support = ROOT / "sdk-support/runtime-lan"
    runtime_source = support / "output_cedar.c"
    known_baseline = "cc208b25d67f1e9853e8444720df838621496bfa9b3db5ef7c24b2b838cf419a"
    if digest(backend_source) not in (known_baseline, digest(runtime_source)):
        raise RuntimeError("Unrecognized DLNA lifecycle source; refusing replacement")
    shutil.copyfile(str(runtime_source), str(backend_source))
    shutil.copyfile(str(support / "dlna_process.h"), str(backend_source.with_name("dlna_process.h")))


def prepare_lan_transport(source):
    candidate = ROOT / "sdk-support/runtime-lan/upnp_transport.c"
    baseline = "24636bfdd0f3d1d593ab085eb6bcbcd255e140945683ca43ad2fd742e7b494cb"
    current = digest(source)
    if current not in (baseline, digest(candidate)):
        raise RuntimeError("Unrecognized DLNA transport source; refusing replacement")
    if current != digest(candidate):
        shutil.copyfile(str(candidate), str(source))


def build_lan(jobs):
    work, host, prefix, env = app_environment()
    directory = work / "SOFTWARE/lan-apps"
    backend_source = directory / "vendor/gmrender/src/output_cedar.c"
    relocate_lan_backend(backend_source)
    prepare_lan_runtime(backend_source)
    prepare_lan_transport(backend_source.with_name("upnp_transport.c"))
    library = directory / "stage"
    sysroot = Path(capture([prefix + "gcc", "--print-sysroot"]))
    env["CC"] = prefix + "gcc"
    env["CFLAGS"] = "-Os"
    for relative in ("vendor/pupnp", "vendor/gmrender"):
        source = directory / relative
        for path in source.rglob("*"):
            if path.is_file() and not path.is_symlink() and path.suffix in (".am", ".ac", ".m4", ".sh", ".c", ".h", ".in"):
                content = path.read_bytes()
                if b"\r\n" in content:
                    path.write_bytes(content.replace(b"\r\n", b"\n"))
        run(["autoreconf", "-fi"], source, env)
        configure = ["./configure", "--host=arm-buildroot-linux-gnueabi", "--prefix=" + str(library)]
        if relative.endswith("pupnp"):
            configure += ["--disable-samples", "--disable-shared", "--enable-static", "--disable-ipv6"]
        else:
            configure += ["--datadir=/opt/f1/apps/lan/data", "--without-gstreamer"]
            env.update({"PKG_CONFIG_LIBDIR": str(library / "lib/pkgconfig") + ":" + str(sysroot / "usr/lib/pkgconfig"),
                        "PKG_CONFIG_SYSROOT_DIR": str(sysroot),
                        "GLIB_CFLAGS": "-I{} -I{}".format(sysroot / "usr/include/glib-2.0", sysroot / "usr/lib/glib-2.0/include"),
                        "GLIB_LIBS": "-lgthread-2.0 -lglib-2.0",
                        "LIBUPNP_CFLAGS": "-I" + str(library / "include/upnp"),
                        "LIBUPNP_LIBS": "{} {} -lpthread".format(library / "lib/libupnp.a", library / "lib/libixml.a")})
        run(configure, source, env)
        run(["make", "-j{}".format(jobs)], source, env)
        if relative.endswith("pupnp"):
            run(["make", "install"], source, env)
    build = directory / "build"
    build.mkdir(exist_ok=True)
    cc = prefix + "gcc"
    run([cc, "-Os", "-std=gnu99", "-D_GNU_SOURCE", "-DNO_SSL", "-DNO_CGI", "-DNO_WEBSOCKET",
         "-DUSE_STACK_SIZE=131072", "-Ivendor/civetweb/include", "-c", "vendor/civetweb/src/civetweb.c",
         "-o", "build/civetweb.o"], directory, env)
    menu = work / "LVGL/menu"
    run(["make", "-j{}".format(jobs), "CC=" + cc, "f1-menu"], menu, env)
    run([cc, "-Os", "-std=gnu99", "-Wall", "-Wextra", "-DLV_CONF_INCLUDE_SIMPLE", "-Ivendor/civetweb/include",
         "-I" + str(menu), "-I" + str(work / "LVGL/touch"), "-I" + str(work / "LVGL/touch/lvgl"),
         "-c", "lan_ui.c", "-o", "build/lan_ui.o"], directory, env)
    run([cc, "-o", "lan-ui", "build/lan_ui.o", "build/civetweb.o", menu / "platform.o", menu / "menu_font_stream.o"] +
        sorted((menu / "build/lvgl").rglob("*.o")) + ["-lm", "-lpthread", "-ldl"], directory, env)
    publish_apps("lan", {"lan-ui": directory / "lan-ui", "gmediarender": directory / "vendor/gmrender/src/gmediarender"}, prefix)
    binary = (OUT / "images/lan/gmediarender").read_bytes()
    if b"/opt/f1/apps/media/mplayer-native\0" not in binary or b"/candidates/" in binary:
        raise RuntimeError("DLNA binary backend path verification failed")
    report_path = OUT / "images/lan/build.json"
    report = json.loads(report_path.read_text())
    report["backend"] = "/opt/f1/apps/media/mplayer-native"
    report["relocated_backend_source_sha256"] = digest(backend_source)
    report_path.write_text(json.dumps(report, indent=2) + "\n")


def prepare_wiliwili_runtime(directory):
    support = ROOT / "sdk-support/runtime-wiliwili"
    files = (
        ("loop_mediaplayer.c", "board_linux/loop_mediaplayer.c",
         "db0f9d62418ead15e064fa7a9133904ee8d17a984b5f2ee939bf7053467e5971"),
        ("main.cpp", "windows_lvgl/src/main.cpp",
         "4737b2895d2a6da87d9c738aeb31c8960c3f917c6f175fc2535525e76e99df42"),
    )
    for name, relative, baseline in files:
        previous = ("7c4a5ee1a5d72f48140f7de2f668a55ba7e68102023709d428173dfbc5b3b02e",) if name == "main.cpp" else ()
        if digest(directory / relative) not in (baseline, digest(support / name)) + previous:
            raise RuntimeError("Unrecognized wiliwili recovery source: " + relative)
    for name, relative, _ in files:
        if digest(directory / relative) != digest(support / name):
            shutil.copyfile(str(support / name), str(directory / relative))
    header = directory / "board_linux/startup_audio_gate.h"
    if not header.is_file() or digest(header) != digest(support / header.name):
        shutil.copyfile(str(support / header.name), str(header))


def prepare_mplayer_frames(source):
    candidate = ROOT / "sdk-support/runtime-media/vd_cedar.c"
    baseline = "6583f9ce5ed462d0e9e17492c3e795a84f76bf7e49a1518870d1d6f00777f4ae"
    current = digest(source)
    if current not in (baseline, digest(candidate)):
        raise RuntimeError("Unrecognized MPlayer Cedar source; refusing replacement")
    if current != digest(candidate):
        shutil.copyfile(str(candidate), str(source))


def build_media_backends(jobs):
    work, host, prefix, env = app_environment()
    _, player, _ = stage("cedar-player")
    _, wili, _ = stage("wiliwili")
    prepare_wiliwili_runtime(wili)
    sysroot = Path(capture([prefix + "gcc", "--print-sysroot"]))
    target = work / "ROOTFS/buildroot-2018.02.11/output/target"
    for name in ("libcdx_parser.so", "libcdx_stream.so", "libcdx_base.so", "libcdx_common.so", "libfdk-aac.so"):
        if not (target / "usr/lib" / name).is_file():
            raise RuntimeError("Build rootfs with CedarX/AAC first: " + name)
    source = player / "player-src"
    cflags = "--sysroot={} -Os -Wall -Wextra -Wno-unused-parameter -D_GNU_SOURCE -DHAVE_CEDARX=1 -I. -I{} -I{} -I{} -MMD -MP".format(
        sysroot, sysroot / "usr/include", sysroot / "usr/include/drm", work / "SOFTWARE/libcedarx")
    run(["make", "-j{}".format(jobs), "CROSS_COMPILE=" + prefix, "SYSROOT=" + str(sysroot),
         "TARGET_DIR=" + str(target), "CFLAGS=" + cflags, "cedar_drm_player"], source, env)
    env["JOBS"] = str(jobs)
    mplayer = work / "SOFTWARE/mplayer-cedar"
    prepare_mplayer_frames(mplayer / "libmpcodecs/vd_cedar.c")
    makefile = mplayer / "Makefile"
    old_include = "CFLAGS += -I/home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/host/arm-buildroot-linux-gnueabi/sysroot/usr/include/drm"
    new_include = "CFLAGS += -I" + str(sysroot / "usr/include/drm")
    content = makefile.read_text()
    if old_include in content:
        if content.count(old_include) != 1:
            raise RuntimeError("Unexpected MPlayer DRM include configuration")
        makefile.write_text(content.replace(old_include, new_include))
    elif new_include not in content:
        raise RuntimeError("Unrecognized generated MPlayer DRM include configuration")
    run(["sh", "build-nosdl.sh"], mplayer, env)
    env["SDK"] = str(host)
    configure = wili / "board_linux/vendor/curl-8.16.0/configure"
    configure.chmod(configure.stat().st_mode | 0o111)
    run(["sh", "build-curl.sh"], wili / "board_linux", env)
    run(["make", "-j{}".format(jobs), "SDK=" + str(host), "PLAYER_SRC=" + str(source),
         "build-arm/wiliwili-lvgl", "net-player"], wili / "board_linux", env)
    publish_apps("media-backends", {"cedar_drm_player": source / "cedar_drm_player",
                                   "mplayer-native": work / "SOFTWARE/mplayer-cedar/mplayer",
                                   "wiliwili-lvgl": wili / "board_linux/build-arm/wiliwili-lvgl",
                                   "cedar-net-player": wili / "board_linux/build-arm/cedar-net-player"}, prefix)


def build_miracast_sink(jobs):
    _, host, prefix, env = app_environment()
    _, source, _ = stage("miracast-sink")
    run(["make", "-f", "Makefile.sink", "-B", "-j{}".format(jobs),
         "CROSS_COMPILE=" + prefix, "TARGET=miracast_sink_dump.lowest.brarm"], source, env)
    publish_apps("miracast-sink", {
        "miracast_sink_dump.lowest.brarm": source / "miracast_sink_dump.lowest.brarm"
    }, prefix)


def runtime_destination(target, relative):
    destination = target / relative
    if Path(relative).is_absolute() or ".." in Path(relative).parts or not contained(destination, target):
        raise RuntimeError("Unsafe runtime destination: " + relative)
    if destination.is_symlink() or any(parent.is_symlink() for parent in destination.parents if parent == target or target in parent.parents):
        raise RuntimeError("Symlink in runtime destination: " + relative)
    return destination


def relocate_runtime_text(content, replacements):
    text = content.decode("utf-8")
    for old, new in sorted(replacements.items(), key=lambda item: (-len(item[0]), item[0])):
        if not old or old not in text:
            raise RuntimeError("Expected runtime relocation input missing: " + old)
        text = text.replace(old, new)
    if "/candidates/" in text:
        raise RuntimeError("Historical candidate path remains in runtime script")
    return text.encode("utf-8")


def runtime_source_plan(layout, target, roots, destinations, known_content=None):
    entries = []
    known_content = dict(known_content or {})
    for spec in layout.get("sources", []):
        root = roots[spec["origin"]]
        source = root / spec["path"]
        if Path(spec["path"]).is_absolute() or ".." in Path(spec["path"]).parts or not contained(source, root):
            raise RuntimeError("Unsafe/missing runtime source: " + str(source))
        if spec["origin"] == "project-git":
            content = subprocess.check_output(["git", "show", MANIFEST["project_sources"]["commit"] + ":" + spec["path"]], cwd=str(ROOT))
        else:
            if source.is_symlink() or not source.is_file() or any(parent.is_symlink() for parent in source.parents if parent == root or root in parent.parents):
                raise RuntimeError("Unsafe/missing runtime source: " + str(source))
            content = source.read_bytes()
        source_hash = hashlib.sha256(content).hexdigest()
        if spec.get("executable") or spec.get("replacements"):
            content = relocate_runtime_text(content, spec.get("replacements", {}))
        for old_hash, relative in sorted(spec.get("checks", {}).items()):
            if not re.fullmatch("[0-9a-f]{32}", old_hash) or relative not in known_content:
                raise RuntimeError("Missing/invalid cast checksum input: " + relative)
            if old_hash.encode("ascii") not in content:
                raise RuntimeError("Expected legacy checksum missing: " + old_hash)
            content = content.replace(old_hash.encode("ascii"), hashlib.md5(known_content[relative]).hexdigest().encode("ascii"))
        if spec.get("executable"):
            subprocess.run(["/bin/sh", "-n"], input=content, check=True)
        for relative in spec["targets"]:
            destination = runtime_destination(target, relative)
            if destination in destinations:
                raise RuntimeError("Duplicate runtime destination: " + relative)
            destinations.add(destination)
            entries.append((spec, destination, content, source_hash))
            known_content[relative] = content
    return entries


def runtime_payload_plan(layout, target):
    entries = []
    destinations = set()
    groups = dict(layout["artifacts"])
    if layout.get("kernel_modules"):
        report = json.loads((OUT / "images/kernel-runtime/build.json").read_text())
        if report["kernel_release"] != MANIFEST["kernel_release"]:
            raise RuntimeError("Kernel runtime report release mismatch")
        groups["kernel-runtime"] = {relative: "lib/modules/{}/kernel/{}".format(MANIFEST["kernel_release"], relative)
                                    for relative in report["modules"]}
    for group, artifacts in sorted(groups.items()):
        images = OUT / "images" / group
        if not contained(images, OUT) or images.is_symlink():
            raise RuntimeError("Unsafe artifact group")
        report = json.loads((images / "build.json").read_text())
        targets = [(name, relative) for name, paths in artifacts.items()
                   for relative in (paths if isinstance(paths, list) else [paths])]
        for name, relative in sorted(targets):
            destination = runtime_destination(target, relative)
            if destination in destinations:
                raise RuntimeError("Duplicate runtime destination: " + relative)
            destinations.add(destination)
            source = images / name
            if not contained(source, images) or source.is_symlink() or not source.is_file():
                raise RuntimeError("Missing/unsafe runtime artifact: " + str(source))
            if "sha256" in report:
                expected = report["sha256"][name]
            else:
                expected = report.get("artifacts", report.get("modules", {}))[name]["sha256"]
            if digest(source) != expected:
                raise RuntimeError("Runtime artifact checksum mismatch: " + str(source))
            entries.append((source, destination, expected))
    return entries


def audit_cast_checks(payload):
    variables = {
        "SELF": "/opt/f1/apps/cast", "P": "/root/aic_miracast", "CLI": "/root/aic_miracast/wpa24_aic_wfd_cli",
        "PLAYER": "/opt/f1/apps/cast/cedar_drm_player", "SINK": "/opt/f1/apps/cast/miracast_sink_dump.lowest",
        "LOADER": "/lib/modules/5.7.1/extra/aic_load_fw.ko", "FDRV": "/lib/modules/5.7.1/extra/aic8800_fdrv.ko",
        "FW_DIR": "/vendor/etc/firmware/aic8800D80", "R": "/opt/f1/apps/cast",
    }
    checked = {}
    for relative in ("opt/f1/apps/cast/start-cast-default-audio.sh", "root/aic_miracast/load-aic-for-miracast.sh"):
        path = payload / relative
        if not path.is_file():
            raise RuntimeError("Missing integrated cast launcher: " + relative)
        for line in path.read_text().splitlines():
            if not line.strip().startswith("check "):
                continue
            args = shlex.split(line)
            if len(args) != 3 or not re.fullmatch("[0-9a-f]{32}", args[2]):
                raise RuntimeError("Unrecognized cast checksum statement")
            name = re.sub(r"\$([A-Z_]+)", lambda match: variables[match.group(1)], args[1])
            if not name.startswith("/"):
                raise RuntimeError("Nonabsolute cast checksum target")
            target = runtime_destination(payload, name[1:])
            if not target.is_file() or hashlib.md5(target.read_bytes()).hexdigest() != args[2]:
                raise RuntimeError("Integrated cast checksum failed: " + name)
            checked[name] = args[2]
    if len(checked) < 13:
        raise RuntimeError("Cast preflight checksum coverage unexpectedly incomplete")
    return checked


def audit_boot_modules(payload):
    script = payload / "etc/init.d/S12board-hardware"
    listed = capture(["/bin/sh", script, "--list-modules"])
    modules = {}
    for line in listed.splitlines():
        name, relative = line.split("|", 1)
        path = runtime_destination(payload, "lib/modules/" + MANIFEST["kernel_release"] + "/" + relative)
        if not path.is_file():
            raise RuntimeError("Boot module missing: " + relative)
        vermagic = capture(["modinfo", "-F", "vermagic", path])
        if not vermagic.startswith(MANIFEST["kernel_release"] + " "):
            raise RuntimeError("Boot module release mismatch")
        if capture(["modinfo", "-F", "name", path]) != name:
            raise RuntimeError("Boot module identity mismatch")
        modules[relative] = {"sha256": digest(path), "vermagic": vermagic}
    if len(modules) != 4:
        raise RuntimeError("Boot module plan incomplete")
    return modules


def build_runtime_payload():
    check_output(create=True)
    target = OUT / "runtime-payload"
    marker = target / ".sdk-runtime-payload"
    if target.exists() and (target.is_symlink() or marker.is_symlink() or not marker.is_file() or marker.read_text() != TOKEN):
        raise RuntimeError("Refusing to overwrite unowned runtime payload")
    layout_path = ROOT / "sdk-support/runtime-layout.json"
    layout = json.loads(layout_path.read_text())
    entries = runtime_payload_plan(layout, target)
    roots = {}
    if layout.get("sources"):
        roots["project"] = stage_project_sources()
        for origin in sorted({spec["origin"] for spec in layout["sources"]} - {"project"}):
            if origin == "project-git":
                roots[origin] = ROOT
                continue
            if origin == "sdk":
                roots[origin] = ROOT / "sdk-support"
                for spec in (spec for spec in layout["sources"] if spec["origin"] == "sdk"):
                    relative = "sdk-support/" + spec["path"]
                    capture(["git", "cat-file", "-e", "HEAD:" + relative], ROOT)
                    if capture(["git", "diff", "--name-only", "HEAD", "--", relative], ROOT):
                        raise RuntimeError("SDK runtime source has uncommitted changes: " + relative)
                continue
            _, roots[origin], _ = stage(origin)
    known_content = {str(destination.relative_to(target)): source.read_bytes() for source, destination, expected in entries}
    sources = runtime_source_plan(layout, target, roots, {entry[1] for entry in entries}, known_content)
    target.mkdir(parents=True, exist_ok=True)
    marker.write_text(TOKEN)
    files = {}
    for source, destination, expected in entries:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(str(source), str(destination))
        destination.chmod(0o644 if destination.suffix in (".ko", ".so") else 0o755)
        if digest(destination) != expected:
            raise RuntimeError("Installed runtime checksum mismatch")
        files[str(destination.relative_to(target))] = expected
    provenance = {}
    for spec, destination, content, source_hash in sources:
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(content)
        destination.chmod(0o755 if spec.get("executable") else 0o644)
        relative = str(destination.relative_to(target))
        files[relative] = digest(destination)
        provenance[relative] = {"origin": spec["origin"], "source_path": spec["path"],
                                "source_sha256": source_hash, "installed_sha256": files[relative],
                                "replacements": spec.get("replacements", {})}
        provenance[relative]["checksum_inputs"] = spec.get("checks", {})
    report = {"layout_sha256": digest(layout_path), "files": files,
              "resource_provenance": provenance,
              "scope": "compiled runtime payload with partial resources/services; base rootfs/full startup/libraries/image integration pending",
              "board_deployed": False}
    if "opt/f1/apps/cast/start-cast-default-audio.sh" in files:
        report["cast_preflight_checks"] = audit_cast_checks(target)
    if "etc/init.d/S12board-hardware" in files:
        report["boot_modules"] = audit_boot_modules(target)
    (OUT / "images/runtime-payload").mkdir(parents=True, exist_ok=True)
    (OUT / "images/runtime-payload/build.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Installed {} source-built runtime artifacts".format(len(files)), flush=True)


def archive_name(name):
    normalized = posixpath.normpath(name)
    if name.startswith("/") or ".." in name.split("/") or normalized.startswith("../"):
        raise RuntimeError("Unsafe rootfs member: " + name)
    return normalized


def merge_rootfs_archive(base_path, payload, files, output_path):
    excluded = {"etc/init.d/S41dhcpcd"}
    with tarfile.open(str(base_path)) as base:
        members = {}
        for member in base.getmembers():
            name = archive_name(member.name)
            if name in members:
                raise RuntimeError("Duplicate base rootfs member: " + name)
            members[name] = member
        if "etc/init.d/S41dhcpcd" in members:
            member = members["etc/init.d/S41dhcpcd"]
            if not member.isfile() or b"DAEMON=/sbin/dhcpcd" not in base.extractfile(member).read():
                raise RuntimeError("Unrecognized global DHCP service; refuse exclusion")
        for relative, expected in files.items():
            if archive_name(relative) != relative:
                raise RuntimeError("Runtime path is not normalized")
            path = runtime_destination(payload, relative)
            if not path.is_file() or digest(path) != expected:
                raise RuntimeError("Runtime file changed before rootfs assembly: " + relative)
            for parent in Path(relative).parents:
                name = parent.as_posix()
                if name in members and not members[name].isdir():
                    raise RuntimeError("Runtime parent collides with base non-directory: " + name)
            if relative in members and not members[relative].isfile():
                raise RuntimeError("Runtime file collides with base special node: " + relative)
        with tarfile.open(str(output_path), "w", format=tarfile.PAX_FORMAT) as output:
            for name, member in sorted(members.items()):
                if name in excluded or name in files:
                    continue
                item = copy.copy(member)
                item.name = name
                if item.islnk():
                    item.linkname = archive_name(item.linkname)
                stream = base.extractfile(member) if member.isfile() else None
                output.addfile(item, stream)
                if stream:
                    stream.close()
            added_directories = set(members)
            for relative in sorted(files):
                for parent in reversed(Path(relative).parents):
                    name = parent.as_posix()
                    if name == "." or name in added_directories:
                        continue
                    item = tarfile.TarInfo(name)
                    item.type = tarfile.DIRTYPE
                    item.mode = 0o755
                    item.uid = item.gid = 0
                    item.uname = item.gname = "root"
                    output.addfile(item)
                    added_directories.add(name)
                path = payload / relative
                item = output.gettarinfo(str(path), arcname=relative)
                item.uid = item.gid = 0
                item.uname = item.gname = "root"
                if relative == "etc/rtl8723bs-sta.conf":
                    item.mode = 0o600
                with path.open("rb") as stream:
                    output.addfile(item, stream)
    return sorted(excluded & set(members))


def resolve_archive_path(members, name):
    name = posixpath.normpath(name).lstrip("/")
    for unused in range(40):
        parts = name.split("/")
        restarted = False
        for index in range(len(parts)):
            prefix = "/".join(parts[:index + 1])
            member = members.get(prefix)
            if member and (member.issym() or member.islnk()):
                link = member.linkname
                parent = "" if member.islnk() or link.startswith("/") else posixpath.dirname(prefix)
                name = posixpath.normpath(posixpath.join(parent, link.lstrip("/"), *parts[index + 1:]))
                if name == ".." or name.startswith("../"):
                    raise RuntimeError("Rootfs link escapes root")
                restarted = True
                break
        if not restarted:
            return name if name in members and members[name].isfile() else None
    raise RuntimeError("Rootfs symlink loop: " + name)


def elf_versions(text):
    definitions = set()
    needs = {}
    section = None
    library = None
    for line in text.splitlines():
        if line.startswith("Version definition section"):
            section = "definitions"
        elif line.startswith("Version needs section"):
            section = "needs"
        elif line.startswith("Version symbols section"):
            section = None
        if section == "definitions" and "Rev:" in line:
            match = re.search(r"\bName:\s+(\S+)", line)
            if match:
                definitions.add(match.group(1))
        elif section == "needs":
            match = re.search(r"\bFile:\s+(\S+)", line)
            if match:
                library = match.group(1)
                needs.setdefault(library, [])
            match = re.search(r"\bName:\s+(\S+)\s+Flags:\s+(\S+)", line)
            if match:
                if not library:
                    raise RuntimeError("ELF version requirement without provider")
                needs[library].append({"name": match.group(1), "weak": match.group(2) == "WEAK"})
    return {"definitions": sorted(definitions), "needs": needs}


def audit_elf_versions(binaries):
    issues = []
    checked = 0
    for name, binary in sorted(binaries.items()):
        for library, requirements in sorted(binary["versions"]["needs"].items()):
            provider = binary["needed"].get(library)
            if provider not in binaries:
                issues.append({"file": name, "error": "missing ELF version provider", "dependency": library})
                continue
            available = set(binaries[provider]["versions"]["definitions"])
            for requirement in requirements:
                checked += 1
                # GNU ld treats an absent weak version requirement as non-fatal.
                if requirement["name"] not in available and not requirement["weak"]:
                    issues.append({"file": name, "error": "missing ELF version definition",
                                   "dependency": library, "provider": provider, "version": requirement["name"]})
    return checked, issues


def audit_rootfs_dependencies(archive_path, readelf):
    issues = []
    binaries = {}
    with tarfile.open(str(archive_path)) as archive, tempfile.TemporaryDirectory() as temporary:
        members = {archive_name(member.name): member for member in archive.getmembers()}
        elf_path = Path(temporary) / "elf"
        for name, member in sorted(members.items()):
            if not member.isfile():
                continue
            with archive.extractfile(member) as stream:
                magic = stream.read(4)
                if magic != b"\x7fELF":
                    continue
                elf_path.write_bytes(magic + stream.read())
            header = capture([readelf, "-h", elf_path])
            if not re.search(r"Machine:\s+ARM\s*$", header, re.MULTILINE):
                issues.append({"file": name, "error": "non-ARM ELF"})
                continue
            dynamic = capture([readelf, "-d", elf_path])
            program = capture([readelf, "-l", elf_path])
            versions = elf_versions(capture([readelf, "--version-info", elf_path]))
            needed = re.findall(r"\(NEEDED\).*\[([^]]+)\]", dynamic)
            interpreter = re.search(r"Requesting program interpreter:\s*([^]]+)\]", program)
            search = ["lib", "usr/lib", "usr/local/lib"]
            for paths in re.findall(r"\((?:RUNPATH|RPATH)\).*\[([^]]+)\]", dynamic):
                for path in paths.split(":"):
                    search.append(path.replace("${ORIGIN}", posixpath.dirname(name)).replace("$ORIGIN", posixpath.dirname(name)).lstrip("/"))
            resolved_dependencies = {}
            for library in needed:
                resolved_path = next((resolve_archive_path(members, posixpath.join(directory, library))
                                      for directory in search if resolve_archive_path(members, posixpath.join(directory, library))), None)
                if not resolved_path:
                    issues.append({"file": name, "error": "missing shared library", "dependency": library})
                else:
                    resolved_dependencies[library] = resolved_path
            if interpreter and not resolve_archive_path(members, interpreter.group(1)):
                issues.append({"file": name, "error": "missing ELF interpreter", "dependency": interpreter.group(1)})
            binaries[name] = {"needed": resolved_dependencies, "interpreter": interpreter.group(1) if interpreter else None,
                              "versions": versions}
    checked, version_issues = audit_elf_versions(binaries)
    issues.extend(version_issues)
    return {"elf_count": len(binaries), "binaries": binaries, "issues": issues,
            "version_requirements_checked": checked,
            "scope": "ARM/interpreter/NEEDED and GNU version definitions/requirements; not full symbol relocation/dlopen or hardware acceptance"}


def build_integrated_rootfs():
    check_output(create=True)
    base_report = json.loads((OUT / "images/rootfs-base/build.json").read_text())
    runtime_report = json.loads((OUT / "images/runtime-payload/build.json").read_text())
    base = OUT / "work/ROOTFS/buildroot-2018.02.11/output/images/rootfs.tar"
    if digest(base) != base_report["rootfs_tar_sha256"]:
        raise RuntimeError("Base rootfs archive checksum mismatch")
    if digest(ROOT / "sdk-support/runtime-layout.json") != runtime_report["layout_sha256"]:
        raise RuntimeError("Runtime layout changed; rebuild runtime-payload")
    images = OUT / "images/rootfs-integrated"
    images.mkdir(parents=True, exist_ok=True)
    temporary = images / "rootfs.tar.pending"
    output = images / "rootfs.tar"
    try:
        excluded = merge_rootfs_archive(base, OUT / "runtime-payload", runtime_report["files"], temporary)
        work, _, prefix, _ = app_environment()
        dependencies = audit_rootfs_dependencies(temporary, prefix + "readelf")
        (images / "dependencies.json").write_text(json.dumps(dependencies, indent=2) + "\n")
        if dependencies["issues"]:
            raise RuntimeError("Rootfs dependency audit failed; see images/rootfs-integrated/dependencies.json")
        os.replace(str(temporary), str(output))
    finally:
        if temporary.exists():
            temporary.unlink()
    report = {"base_rootfs_sha256": digest(base), "runtime_report_sha256": digest(OUT / "images/runtime-payload/build.json"),
              "rootfs_tar_sha256": digest(output), "rootfs_tar_bytes": output.stat().st_size,
              "excluded_services": excluded, "runtime_files": len(runtime_report["files"]),
              "dependency_report_sha256": digest(images / "dependencies.json"), "elf_count": dependencies["elf_count"],
              "scope": "assembled rootfs candidate; full startup/dependency/image/hardware validation pending",
              "board_deployed": False}
    (images / "build.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


def build_fat_tools(jobs, prefix, env):
    archive = ROOT / "ROOTFS/buildroot-2018.02.11/dl/dosfstools-4.1.tar.xz"
    if digest(archive) != "e6b2aca70ccc3fe3687365009dd94a2e18e82b688ed4e260e04b7412471cc173":
        raise RuntimeError("dosfstools source checksum mismatch")
    parent = OUT / "runtime-tools-build"
    source = parent / "dosfstools-4.1"
    if not source.exists():
        with archive.open("rb") as stream:
            extract_archive(stream, parent)
    if not (source / "Makefile").is_file():
        configure_env = dict(env, CC=prefix + "gcc", CFLAGS="-Os")
        run([source / "configure", "--host=arm-buildroot-linux-gnueabi", "--without-udev",
             "--disable-compat-symlinks"], source, configure_env)
    run(["make", "-j{}".format(jobs), "-C", "src", "mkfs.fat", "fsck.fat"], source, env)
    return {name: source / "src" / name for name in ("mkfs.fat", "fsck.fat")}


def build_runtime_tools(jobs):
    work, _, prefix, env = app_environment()
    network = work / "SCRCPT/network-and-audio-source"
    sources = {
        "f1-rtc-time-ui": network / "rtc_time_20261003/f1-rtc-time.c",
        "f1-ip-config": network / "static_ip_20261004/f1-ip-config.c",
        "f1-app-runner": ROOT / "sdk-support/runtime-menu/f1_app_runner.c",
        "f1-data-partition": ROOT / "sdk-support/runtime-data/f1_data_partition.c",
    }
    generated = OUT / "runtime-tools-build"
    generated.mkdir(parents=True, exist_ok=True)
    outputs = {}
    for name, source in sources.items():
        if not source.is_file():
            raise RuntimeError("Missing runtime tool source: " + str(source))
        output = generated / name
        host_output = generated / (name + ".host")
        if not output.is_file() or source.stat().st_mtime > output.stat().st_mtime:
            run([prefix + "gcc", "-Os", "-march=armv5te", "-std=gnu99", "-Wall", "-Wextra", "-Werror",
                 source, "-o", output], env=env)
        if not host_output.is_file() or source.stat().st_mtime > host_output.stat().st_mtime:
            run(["gcc", "-Os", "-std=gnu99", "-Wall", "-Wextra", "-Werror", source,
                 "-o", host_output], env=env)
        outputs[name] = output
    run([generated / "f1-rtc-time-ui.host", "--selftest"], env=env)
    run([generated / "f1-ip-config.host", "validate", "static", "10.0.0.234/24", "10.0.0.1", "10.0.0.1"], env=env)
    invalid = [
        ["static", "10.0.0.234/33", "10.0.0.1", "10.0.0.1"],
        ["static", "10.0.0.234/24", "192.168.1.1", "10.0.0.1"],
        ["static", "not-an-ip/24", "10.0.0.1", "10.0.0.1"],
    ]
    for arguments in invalid:
        result = subprocess.run([str(generated / "f1-ip-config.host"), "validate"] + arguments, env=env)
        if result.returncode != 3:
            raise RuntimeError("IP validation failed to reject invalid configuration")
    outputs.update(build_fat_tools(jobs, prefix, env))
    publish_apps("runtime-tools", outputs, prefix)


def stage_protocol_source():
    spec = MANIFEST["protocol_source"]
    archive_path = ROOT / spec["archive"]
    if not contained(archive_path, ROOT) or archive_path.is_symlink() or digest(archive_path) != spec["sha256"]:
        raise RuntimeError("Protocol source archive checksum/path mismatch")
    check_output(create=True)
    target = OUT / "sources/wfd-protocol"
    stamp = target / ".sdk-source-commit"
    if target.exists():
        if target.is_symlink() or stamp.is_symlink() or not stamp.is_file() or stamp.read_text().strip() != spec["sha256"]:
            raise RuntimeError("Unowned or changed protocol stage")
        return target
    target.mkdir(parents=True)
    try:
        with tarfile.open(str(archive_path)) as archive:
            root = spec["member_root"] + "/"
            for member in archive.getmembers():
                if not member.name.startswith(root):
                    continue
                relative = member.name[len(root):]
                if not relative or member.isdir():
                    continue
                destination = target / relative
                if not contained(destination, target) or ".." in Path(relative).parts or not member.isfile():
                    raise RuntimeError("Unsafe protocol archive member: " + member.name)
                if Path(relative).suffix in (".o", ".d", ".a", ".so") or Path(relative).name in ("wpa_supplicant", "wpa_cli", "wpa_passphrase"):
                    continue
                destination.parent.mkdir(parents=True, exist_ok=True)
                with archive.extractfile(member) as stream, destination.open("wb") as output:
                    shutil.copyfileobj(stream, output)
        for relative in ("wpa_supplicant/Makefile", "wpa_supplicant/.config", "src/p2p/p2p.c"):
            if not (target / relative).is_file():
                raise RuntimeError("Protocol source missing: " + relative)
        stamp.write_text(spec["sha256"] + "\n")
    except BaseException:
        shutil.rmtree(str(target))
        raise
    return target


def build_wfd_protocol(jobs):
    _, host, prefix, env = app_environment()
    source = stage_protocol_source()
    directory = source / "wpa_supplicant"
    sysroot = host / "arm-buildroot-linux-gnueabi/sysroot"
    env.update({"PKG_CONFIG_SYSROOT_DIR": str(sysroot),
                "PKG_CONFIG_LIBDIR": str(sysroot / "usr/lib/pkgconfig"),
                "PKG_CONFIG": str(host / "bin/pkg-config"),
                "CFLAGS": "-Os -march=armv5te -I" + str(sysroot / "usr/include/libnl3"),
                "LDFLAGS": "-L" + str(sysroot / "usr/lib")})
    run(["make", "-B", "-j{}".format(jobs), "CC=" + prefix + "gcc",
         "wpa_supplicant", "wpa_cli", "wpa_passphrase"], directory, env)
    publish_apps("wfd-protocol", {name: directory / name for name in ("wpa_supplicant", "wpa_cli", "wpa_passphrase")}, prefix)
    report_path = OUT / "images/wfd-protocol/build.json"
    report = json.loads(report_path.read_text())
    report["protocol_source"] = MANIFEST["protocol_source"]
    report["input_config_sha256"] = digest(directory / ".config")
    report_path.write_text(json.dumps(report, indent=2) + "\n")


def build_bluetooth_audio(jobs):
    work, host, prefix, env = app_environment()
    _, source, _ = stage("bluealsa")
    sysroot = host / "arm-buildroot-linux-gnueabi/sysroot"
    env.update({"CC": prefix + "gcc", "AR": prefix + "ar", "RANLIB": prefix + "ranlib",
                "PKG_CONFIG": str(host / "bin/pkg-config"),
                "PKG_CONFIG_SYSROOT_DIR": str(sysroot),
                "PKG_CONFIG_LIBDIR": str(sysroot / "usr/lib/pkgconfig"),
                "CFLAGS": "-Os -march=armv5te"})
    if not (source / "configure").is_file():
        run(["autoreconf", "-fi"], source, env)
    run(["/bin/sh", "configure", "--host=arm-buildroot-linux-gnueabi", "--prefix=/usr",
         "--disable-debug", "--with-dbusconfdir=/etc/dbus-1/system.d",
         "--with-alsaplugindir=/usr/lib/alsa-lib", "--with-alsaconfdir=/usr/share/alsa/alsa.conf.d"], source, env)
    run(["make", "-j{}".format(jobs)], source, env)
    install = source / "sdk-install"
    run(["make", "install", "DESTDIR=" + str(install)], source, env)
    route = work / "SCRCPT/network-and-audio-source/audio_route_20260930"
    flags = shlex.split(capture([host / "bin/pkg-config", "--cflags", "--libs", "dbus-1"], env=env))
    output = route / "f1-audio-route"
    run([prefix + "gcc", "-std=gnu99", "-Wall", "-Wextra", "-Os", "-march=armv5te",
         "-I", route, route / "audio-route.c", route / "shared/dbus-client.c"] + flags +
        ["-lbluetooth", "-o", output], route, env)
    publish_apps("bluetooth-audio", {
        "bluealsa": install / "usr/bin/bluealsa",
        "bluealsa-aplay": install / "usr/bin/bluealsa-aplay",
        "libasound_module_pcm_bluealsa.so": install / "usr/lib/alsa-lib/libasound_module_pcm_bluealsa.so",
        "libasound_module_ctl_bluealsa.so": install / "usr/lib/alsa-lib/libasound_module_ctl_bluealsa.so",
        "f1-audio-route": output,
    }, prefix)


def sd_partition_table():
    return [
        {"name": "boot", "start": 2048, "sectors": 32768, "type": 0x0e},
        {"name": "rootfs", "start": 34816, "sectors": 1013760, "type": 0x83},
    ]


def sd_mbr(partitions, total_sectors, bootloader_bytes):
    if len(partitions) != 2 or total_sectors < 1 or bootloader_bytes < 1:
        raise RuntimeError("Invalid SD image layout")
    previous = 0
    result = bytearray(512)
    struct.pack_into("<I", result, 440, 0xf1c20050)
    for index, partition in enumerate(partitions):
        start, length = partition["start"], partition["sectors"]
        if start < previous or length < 1 or start + length > total_sectors:
            raise RuntimeError("Overlapping/out-of-bounds SD partitions")
        if index == 0 and (start < 2048 or 8192 + bootloader_bytes > start * 512):
            raise RuntimeError("U-Boot overlaps the first partition")
        struct.pack_into("<B3sB3sII", result, 446 + index * 16, 0x80 if index == 0 else 0,
                         b"\xfe\xff\xff", partition["type"], b"\xfe\xff\xff", start, length)
        previous = start + length
    result[510:512] = b"\x55\xaa"
    return bytes(result)


def file_region_hash(path, offset, length):
    sha = hashlib.sha256()
    with path.open("rb") as stream:
        stream.seek(offset)
        while length:
            block = stream.read(min(length, 1024 * 1024))
            if not block:
                raise RuntimeError("Truncated image region")
            sha.update(block)
            length -= len(block)
    return sha.hexdigest()


def build_sd_image():
    check_output(create=True)
    images = OUT / "images/sd-image"
    images.mkdir(parents=True, exist_ok=True)
    rootfs = OUT / "images/rootfs-integrated/rootfs.tar"
    report = json.loads((rootfs.parent / "build.json").read_text())
    if digest(rootfs) != report["rootfs_tar_sha256"]:
        raise RuntimeError("Integrated rootfs checksum mismatch")
    dependencies = json.loads((rootfs.parent / "dependencies.json").read_text())
    if digest(rootfs.parent / "dependencies.json") != report["dependency_report_sha256"] or dependencies["issues"]:
        raise RuntimeError("Integrated dependency audit missing/failed")
    inputs = {
        "uboot": OUT / "images/uboot/u-boot-sunxi-with-spl.bin",
        "kernel": OUT / "images/kernel/zImage",
        "dtb": OUT / "images/board/suniv-f1c100s-licheepi-nano.dtb",
    }
    for name in ("uboot", "kernel"):
        source_report = json.loads((OUT / "images" / name / "build.json").read_text())
        if digest(inputs[name]) != source_report["sha256"][inputs[name].name]:
            raise RuntimeError("Boot input checksum mismatch: " + name)
    if digest(inputs["dtb"]) != MANIFEST["project_sources"]["dtb_sha256"]:
        raise RuntimeError("Boot DTB mismatch")
    host = OUT / "work/ROOTFS/buildroot-2018.02.11/output/host"
    tool_report = json.loads((OUT / "images/image-tools/build.json").read_text())
    for name, spec in tool_report["tools"].items():
        if digest(OUT / spec["path"]) != spec["sha256"]:
            raise RuntimeError("Image tool checksum mismatch: " + name)
    partitions = sd_partition_table()
    total_sectors = partitions[-1]["start"] + partitions[-1]["sectors"]
    mbr = sd_mbr(partitions, total_sectors, inputs["uboot"].stat().st_size)
    with tempfile.TemporaryDirectory(prefix="sd-build-", dir=str(images)) as temporary:
        stage = Path(temporary)
        boot_script = stage / "boot.scr"
        run([OUT / "sources/uboot/tools/mkimage", "-A", "arm", "-O", "linux", "-T", "script",
             "-C", "none", "-n", "F1C200S SDK", "-d", ROOT / "sdk-support/boot.cmd", boot_script])
        filesystems = {}
        for partition in partitions:
            path = stage / (partition["name"] + ".img")
            with path.open("wb") as stream:
                stream.truncate(partition["sectors"] * 512)
            filesystems[partition["name"]] = path
        run(["mkfs.vfat", "-F", "16", "-n", "F1BOOT", "-h", str(partitions[0]["start"]), filesystems["boot"]])
        for path, name in ((inputs["kernel"], "zImage"), (inputs["dtb"], "suniv-f1c100s-licheepi-nano.dtb"), (boot_script, "boot.scr")):
            run(["mcopy", "-i", filesystems["boot"], path, "::" + name])
            recovered = stage / ("verify-" + name)
            run(["mcopy", "-i", filesystems["boot"], "::" + name, recovered])
            if digest(path) != digest(recovered):
                raise RuntimeError("Boot FAT file verification failed: " + name)
        root = stage / "root"
        root.mkdir()
        command = " && ".join([
            "tar -xpf {} -C {}".format(shlex.quote(str(rootfs)), shlex.quote(str(root))),
            "mkdir -p " + shlex.quote(str(root / "root/roms")),
            "{} -t ext2 -F -b 4096 -I 256 -m 0 -L F1ROOT -d {} {}".format(
                shlex.quote(str(host / "sbin/mke2fs")), shlex.quote(str(root)), shlex.quote(str(filesystems["rootfs"]))),
        ])
        run([host / "bin/fakeroot", "/bin/sh", "-c", command])
        run([host / "sbin/e2fsck", "-fn", filesystems["rootfs"]])
        console = capture([host / "sbin/debugfs", "-R", "stat /dev/console", filesystems["rootfs"]])
        if "character special" not in console or not re.search(r"User:\s+0\s+Group:\s+0", console):
            raise RuntimeError("Rootfs console node/ownership incorrect")
        for name in ("boot",):
            run(["fsck.vfat", "-n", filesystems[name]])
        output = images / "f1c200s-sdk-candidate.img"
        pending = images / "f1c200s-sdk-candidate.img.pending"
        try:
            with pending.open("wb") as stream:
                stream.truncate(total_sectors * 512)
                stream.seek(0)
                stream.write(mbr)
                stream.seek(8192)
                with inputs["uboot"].open("rb") as source:
                    shutil.copyfileobj(source, stream)
                for partition in partitions:
                    stream.seek(partition["start"] * 512)
                    with filesystems[partition["name"]].open("rb") as source:
                        shutil.copyfileobj(source, stream)
                stream.flush()
                os.fsync(stream.fileno())
            partition_hashes = {}
            for partition in partitions:
                expected = digest(filesystems[partition["name"]])
                actual = file_region_hash(pending, partition["start"] * 512, partition["sectors"] * 512)
                if actual != expected:
                    raise RuntimeError("SD image partition byte verification failed")
                partition_hashes[partition["name"]] = actual
            if file_region_hash(pending, 8192, inputs["uboot"].stat().st_size) != digest(inputs["uboot"]):
                raise RuntimeError("SD U-Boot region mismatch")
            os.replace(str(pending), str(output))
        finally:
            if pending.exists():
                pending.unlink()
    report = {"image": output.name, "sha256": digest(output), "bytes": output.stat().st_size,
              "sdk_version": (ROOT / "VERSION").read_text().strip(),
              "first_boot_data": "FAT32 p3 uses remaining card space; initializer self-removes and reboots",
              "partitions": partitions, "partition_sha256": partition_hashes,
              "rootfs_tar_sha256": digest(rootfs), "boot_input_sha256": {name: digest(path) for name, path in inputs.items()},
              "offline_filesystem_checks": "passed", "board_deployed": False, "cold_boots": 0,
              "scope": "flash-layout candidate; portable full build/clean and hardware acceptance pending"}
    (images / "build.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


def build_image_tools(jobs):
    original, target, spec = stage("rootfs")
    env = os.environ.copy()
    env["BR2_DL_DIR"] = str(original / "dl")
    env["LC_ALL"] = "C"
    run(["make", "-j{}".format(jobs), "host-e2fsprogs"], target, env)
    host = target / "output/host"
    tools = {}
    for name in ("mke2fs", "e2fsck", "debugfs"):
        path = host / "sbin" / name
        if not path.is_file():
            raise RuntimeError("Missing source-built image tool: " + name)
        tools[name] = {"path": str(path.relative_to(OUT)), "sha256": digest(path)}
    images = OUT / "images/image-tools"
    images.mkdir(parents=True, exist_ok=True)
    report = {"buildroot_commit": spec["commit"], "tools": tools, "board_deployed": False}
    (images / "build.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


def prepare_busybox_config(target):
    source = target / "package/busybox/busybox.config"
    output = target / "sdk-busybox.config"
    previous = output.read_bytes() if output.is_file() else None
    shutil.copy2(str(source), str(output))
    set_config(output, {"CONFIG_UDHCPD": "y"})
    return previous != output.read_bytes()


def build_rootfs(jobs, sources_only=False):
    original, target, spec = stage("rootfs")
    _, cedarc, _ = stage("libcedarc")
    _, cedarx, _ = stage("libcedarx")
    busybox_changed = prepare_busybox_config(target)
    previous_config = config_values((target / ".config").read_text())
    # The legacy overlay is deliberately not part of this base-rootfs recipe.
    # Current board files are integrated separately, never collected from old payloads.
    set_config(target / ".config", {
        "BR2_ROOTFS_OVERLAY": '""',
        "BR2_ROOTFS_POST_BUILD_SCRIPT": '""',
        "BR2_ROOTFS_POST_FAKEROOT_SCRIPT": '""',
        "BR2_ROOTFS_POST_IMAGE_SCRIPT": '""',
        "BR2_PACKAGE_OVERRIDE_FILE": '"$(CONFIG_DIR)/local.mk"',
        "BR2_PACKAGE_BUSYBOX_CONFIG": '"$(CONFIG_DIR)/sdk-busybox.config"',
        "BR2_WGET": '"wget --passive-ftp -nd --timeout=30 --tries=1"',
        "BR2_PACKAGE_LIBDRM": "y",
        "BR2_PACKAGE_LIBCEDARX": "y",
        "BR2_PACKAGE_FDK_AAC": "y",
        "BR2_PACKAGE_FAAD2": "y",
        "BR2_PACKAGE_LIBMAD": "y",
        "BR2_PACKAGE_BLUEZ5_UTILS_DEPRECATED": "y",
        "BR2_PACKAGE_BLUEZ5_UTILS_CLIENT": "y",
    })
    (target / "local.mk").write_text("LIBCEDARC_OVERRIDE_SRCDIR = {}\nLIBCEDARX_OVERRIDE_SRCDIR = {}\n".format(cedarc, cedarx))
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
    bluez_options = ("BR2_PACKAGE_BLUEZ5_UTILS_CLIENT", "BR2_PACKAGE_BLUEZ5_UTILS_DEPRECATED")
    if any(selected.get(key) != "y" for key in bluez_options):
        raise RuntimeError("Required BlueZ client/legacy tools disappeared after configuration")
    if not sources_only and any(previous_config.get(key) != selected.get(key) for key in bluez_options) and (target / "output/build/bluez5_utils-5.48/.stamp_configured").is_file():
        run(["make", "-j{}".format(jobs), "bluez5_utils-reconfigure"], target, env)
    if not sources_only and busybox_changed and (target / "output/build/busybox-1.27.2/.stamp_configured").is_file():
        run(["make", "busybox-reconfigure"], target, env)
    run(["make", "-j{}".format(jobs), "source" if sources_only else "all"], target, env)
    if not sources_only:
        artifact = target / "output/images/rootfs.tar"
        if not artifact.is_file():
            raise RuntimeError("Buildroot did not produce the base rootfs archive")
        with tarfile.open(str(artifact)) as archive:
            members = {posixpath.normpath(member.name): member for member in archive.getmembers()}
        for required in ("bin/busybox", "sbin/init", "etc/inittab", "etc/passwd", "etc/shadow",
                         "usr/sbin/dropbear", "usr/sbin/udhcpd", "usr/bin/aplay", "usr/bin/hciconfig", "usr/libexec/bluetooth/bluetoothd"):
            if required not in members:
                raise RuntimeError("Required base rootfs file missing: " + required)
        compiler = target / "output/host/bin/arm-buildroot-linux-gnueabi-gcc"
        images = OUT / "images/rootfs-base"
        images.mkdir(parents=True, exist_ok=True)
        report = {"source_commit": spec["commit"], "input_config_sha256": spec["config_sha256"],
                  "effective_config_sha256": digest(target / ".config"),
                  "busybox_config_sha256": digest(target / "sdk-busybox.config"),
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


def full_build_steps(jobs, image=False):
    steps = [
        ("rootfs-base", lambda: build_rootfs(jobs)),
        ("kernel", lambda: build_boot("kernel", jobs)),
        ("uboot", lambda: build_boot("uboot", jobs)),
        ("board", lambda: build_board(jobs)),
        ("wireless-modules", lambda: build_wireless_modules(jobs)),
        ("kernel-runtime", build_kernel_runtime),
        ("apps-ui", lambda: build_app_ui(jobs)),
        ("ons", lambda: build_ons(jobs)),
        ("retro-cores", lambda: build_retro_cores(jobs)),
        ("lan", lambda: build_lan(jobs)),
        ("media-backends", lambda: build_media_backends(jobs)),
        ("miracast-sink", lambda: build_miracast_sink(jobs)),
        ("bluetooth-audio", lambda: build_bluetooth_audio(jobs)),
        ("wfd-protocol", lambda: build_wfd_protocol(jobs)),
        ("runtime-tools", lambda: build_runtime_tools(jobs)),
    ]
    if image:
        steps += [("image-tools", lambda: build_image_tools(jobs)),
                  ("runtime-payload", build_runtime_payload),
                  ("rootfs-integrated", build_integrated_rootfs),
                  ("sd-image", build_sd_image)]
    return steps


def incremental_inputs(name):
    # Locked source/config identities, recipes and helper sources determine reuse.
    inputs = {"manifest": MANIFEST, "component": name, "support": {},
              "toolchain": os.environ.get("KERNEL_TOOLCHAIN", "default")}
    # Dirty locked sources must invalidate reuse and reach the existing guards.
    inputs["project_diff"] = capture(["git", "diff", MANIFEST["project_sources"]["commit"], "--"] +
                                     MANIFEST["project_sources"]["paths"], ROOT)
    for relative in capture(["git", "ls-files", "sdk-support"], ROOT).splitlines():
        path = ROOT / relative
        if path.is_file() and path.suffix not in (".md",) and path.name != "test_sdk.py":
            inputs["support"][relative] = digest(path)
    inputs["dependencies"] = {}
    for report in sorted((OUT / "images").glob("*/build.json")):
        group = report.parent.name
        # Only earlier steps may invalidate this stage; never depend on itself.
        order = [key for key, unused in full_build_steps(1, image=True)]
        if name in order and group in order and order.index(group) < order.index(name):
            inputs["dependencies"][group] = digest(report)
    return inputs


def incremental_outputs(name):
    directory = OUT / "images" / name
    if not (directory / "build.json").is_file():
        return None
    result = {}
    for path in sorted(directory.rglob("*")):
        if path.is_symlink():
            return None
        if path.is_file():
            stat = path.stat()
            result[str(path.relative_to(OUT))] = [stat.st_size, stat.st_mtime_ns]
    return result


def incremental_build(name, operation):
    check_output(create=True)
    stamps = OUT / "stamps/incremental"
    stamps.mkdir(parents=True, exist_ok=True)
    stamp = stamps / (name + ".json")
    inputs = incremental_inputs(name)
    outputs = incremental_outputs(name)
    if stamp.is_file():
        previous = json.loads(stamp.read_text())
        if outputs and previous == {"inputs": inputs, "outputs": outputs}:
            print("UP TO DATE: " + name, flush=True)
            return
        stamp.unlink()
    print("INCREMENTAL BUILD: " + name, flush=True)
    operation()
    outputs = incremental_outputs(name)
    if outputs:
        stamp.write_text(json.dumps({"inputs": inputs, "outputs": outputs}, sort_keys=True) + "\n")


def build_all(jobs, image=False):
    if doctor(release=False):
        raise RuntimeError("Full build source/environment preflight failed")
    check_output(create=True)
    images = OUT / "images/full-build"
    images.mkdir(parents=True, exist_ok=True)
    path = images / "build.json"
    report = {"build_complete": False, "hardware_accepted": False, "completed_steps": [],
              "manifest_sha256": digest(ROOT / "sdk-support/manifest.json"),
              "recipe_sha256": digest(ROOT / "sdk-support/sdk.py"), "jobs": jobs,
              "board_deployed": False}
    for name, operation in full_build_steps(jobs, image=image):
        report["current_step"] = name
        path.write_text(json.dumps(report, indent=2) + "\n")
        print("BUILD STEP: " + name, flush=True)
        try:
            incremental_build(name, operation)
        except BaseException as error:
            report["error"] = str(error)
            path.write_text(json.dumps(report, indent=2) + "\n")
            raise
        report["completed_steps"].append(name)
    if not image:
        report.update({"build_complete": True, "image_requested": False, "current_step": "complete"})
        path.write_text(json.dumps(report, indent=2) + "\n")
        print("Incremental compilation complete; image packaging not requested", flush=True)
        return
    image_report_path = OUT / "images/sd-image/build.json"
    image_report = json.loads(image_report_path.read_text())
    image_path = image_report_path.parent / image_report["image"]
    if digest(image_path) != image_report["sha256"]:
        raise RuntimeError("Full build final image checksum mismatch")
    report.update({"build_complete": True, "image_sha256": image_report["sha256"],
                   "image": str(image_path.relative_to(OUT)),
                   "image_report_sha256": digest(image_report_path), "current_step": "complete"})
    path.write_text(json.dumps(report, indent=2) + "\n")
    print("Full candidate image build completed; hardware acceptance remains pending: " + str(image_path), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", action="version", version=(ROOT / "VERSION").read_text().strip())
    sub = parser.add_subparsers(dest="command")
    inspect = sub.add_parser("doctor")
    sub.add_parser("package", help="Export complete source, toolchain and dependencies to .sdk-output/portable-sdk")
    inspect.add_argument("--release", action="store_true")
    build = sub.add_parser("build")
    build.add_argument("component", choices=["all", "kernel", "uboot", "rootfs-sources", "rootfs-base", "board", "apps-ui", "ons", "retro-cores", "lan", "media-backends", "miracast-sink", "bluetooth-audio", "wireless-modules", "wfd-protocol", "runtime-tools", "runtime-payload", "kernel-runtime", "rootfs-integrated", "image-tools", "sd-image"], default="all", nargs="?")
    build.add_argument("-j", "--jobs", type=int, default=8)
    build.add_argument("--image", action="store_true", help="Explicitly assemble rootfs and SD image after compilation")
    clean = sub.add_parser("clean")
    clean.add_argument("--component", choices=sorted(MANIFEST["components"]))
    args = parser.parse_args()
    if not args.command:
        parser.error("a command is required")
    if args.command == "build":
        if args.image and args.component not in ("all", "runtime-payload", "rootfs-integrated", "sd-image", "image-tools"):
            parser.error("--image is supported with all or a packaging component")
        if args.component in ("runtime-payload", "rootfs-integrated", "sd-image", "image-tools") and not args.image:
            parser.error("packaging requires explicit --image")
    with locked():
        if args.command == "package":
            from package_sdk import build_package
            build_package(sys.modules[__name__])
            return 0
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
            build_all(args.jobs, image=args.image)
            return 0
        operations = dict(full_build_steps(args.jobs, image=True))
        if args.component in operations:
            incremental_build(args.component, operations[args.component])
            return 0
        if args.component in ("rootfs-sources", "rootfs-base"):
            build_rootfs(args.jobs, sources_only=args.component == "rootfs-sources")
            return 0
        if args.component == "board":
            build_board(args.jobs)
            return 0
        if args.component == "apps-ui":
            build_app_ui(args.jobs)
            return 0
        if args.component == "ons":
            build_ons(args.jobs)
            return 0
        if args.component == "retro-cores":
            build_retro_cores(args.jobs)
            return 0
        if args.component == "lan":
            build_lan(args.jobs)
            return 0
        if args.component == "media-backends":
            build_media_backends(args.jobs)
            return 0
        if args.component == "miracast-sink":
            build_miracast_sink(args.jobs)
            return 0
        if args.component == "bluetooth-audio":
            build_bluetooth_audio(args.jobs)
            return 0
        if args.component == "wireless-modules":
            build_wireless_modules(args.jobs)
            return 0
        if args.component == "wfd-protocol":
            build_wfd_protocol(args.jobs)
            return 0
        if args.component == "runtime-tools":
            build_runtime_tools(args.jobs)
            return 0
        if args.component == "runtime-payload":
            build_runtime_payload()
            return 0
        if args.component == "kernel-runtime":
            build_kernel_runtime()
            return 0
        if args.component == "rootfs-integrated":
            build_integrated_rootfs()
            return 0
        if args.component == "image-tools":
            build_image_tools(args.jobs)
            return 0
        if args.component == "sd-image":
            build_sd_image()
            return 0
        build_boot(args.component, args.jobs)
        return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print("SDK failed: {}".format(error), file=sys.stderr)
        sys.exit(1)
