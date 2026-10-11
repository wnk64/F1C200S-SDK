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


def build_lan(jobs):
    work, host, prefix, env = app_environment()
    directory = work / "SOFTWARE/lan-apps"
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


def build_media_backends(jobs):
    work, host, prefix, env = app_environment()
    _, player, _ = stage("cedar-player")
    _, wili, _ = stage("wiliwili")
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


def build_rootfs(jobs, sources_only=False):
    original, target, spec = stage("rootfs")
    _, cedarc, _ = stage("libcedarc")
    _, cedarx, _ = stage("libcedarx")
    # The legacy overlay is deliberately not part of this base-rootfs recipe.
    # Current board files are integrated separately, never collected from old payloads.
    set_config(target / ".config", {
        "BR2_ROOTFS_OVERLAY": '""',
        "BR2_ROOTFS_POST_BUILD_SCRIPT": '""',
        "BR2_ROOTFS_POST_FAKEROOT_SCRIPT": '""',
        "BR2_ROOTFS_POST_IMAGE_SCRIPT": '""',
        "BR2_PACKAGE_OVERRIDE_FILE": '"$(CONFIG_DIR)/local.mk"',
        "BR2_WGET": '"wget --passive-ftp -nd --timeout=30 --tries=1"',
        "BR2_PACKAGE_LIBDRM": "y",
        "BR2_PACKAGE_LIBCEDARX": "y",
        "BR2_PACKAGE_FDK_AAC": "y",
        "BR2_PACKAGE_FAAD2": "y",
        "BR2_PACKAGE_LIBMAD": "y",
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
    build.add_argument("component", choices=["all", "kernel", "uboot", "rootfs-sources", "rootfs-base", "board", "apps-ui", "ons", "retro-cores", "lan", "media-backends", "miracast-sink", "bluetooth-audio", "wireless-modules", "wfd-protocol"], default="all", nargs="?")
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
        build_boot(args.component, args.jobs)
        return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print("SDK failed: {}".format(error), file=sys.stderr)
        sys.exit(1)
