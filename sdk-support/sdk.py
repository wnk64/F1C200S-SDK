#!/usr/bin/env python3
"""Source-locked F1C200S SDK. Generated files never replace active build trees."""
import argparse
import contextlib
import fcntl
import hashlib
import json
import os
from pathlib import Path
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


def digest(path):
    sha = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            sha.update(block)
    return sha.hexdigest()


def capture(args, cwd=None, env=None):
    return subprocess.check_output(args, cwd=cwd, env=env, universal_newlines=True).strip()


def run(args, cwd=None, env=None):
    args = list(map(str, args))
    print("+", " ".join(args), flush=True)
    subprocess.run(args, cwd=cwd, env=env, check=True)


def contained(path, parent):
    return path.resolve() != parent.resolve() and parent.resolve() in path.resolve().parents


def component(name):
    spec = MANIFEST["components"][name]
    path = ROOT / spec["path"]
    if not contained(path, ROOT) or path.is_symlink():
        raise RuntimeError(f"Source must be a real directory inside SDK: {path}")
    return path, spec


def validate_source(name):
    path, spec = component(name)
    if capture(["git", "rev-parse", "--show-toplevel"], path) != str(path):
        raise RuntimeError(f"Missing complete component repository: {name}")
    if capture(["git", "rev-parse", "HEAD"], path) != spec["commit"]:
        raise RuntimeError(f"Unreviewed source commit: {name}; review manifest before updating")
    if capture(["git", "diff", "--name-only", "HEAD", "--"], path):
        raise RuntimeError(f"Tracked source differs from locked commit: {name}")
    if digest(path / ".config") != spec["config_sha256"]:
        raise RuntimeError(f"Configuration hash mismatch: {name}")
    if "baseline_sha256" in spec:
        if digest(path / spec["artifact"]) != spec["baseline_sha256"]:
            raise RuntimeError(f"Active baseline artifact mismatch: {name}")
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
                raise RuntimeError(f"Expected Linaro GCC 7.2.1, got {version}")
            return path.resolve()
    raise RuntimeError("Linaro 7.2.1 missing; set KERNEL_TOOLCHAIN to its bin directory")


def check_output(create=False):
    if OUT.is_symlink() or OUT.resolve() != ROOT / ".sdk-output":
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
    descriptor = os.open(ROOT, os.O_RDONLY)
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
                raise RuntimeError(f"Archive path escapes stage: {member.name}")
            if member.issym() and not contained(destination.parent / member.linkname, target):
                raise RuntimeError(f"Archive symlink escapes stage: {member.name}")
            if member.islnk() and not contained(target / member.linkname, target):
                raise RuntimeError(f"Archive hardlink escapes stage: {member.name}")
            if not (member.isfile() or member.isdir() or member.issym() or member.islnk()):
                raise RuntimeError(f"Unsupported archive member: {member.name}")
        tree.extractall(target)


def stage(name):
    source, spec = validate_source(name)
    check_output(create=True)
    target = OUT / "sources" / name
    stamp = target / ".sdk-source-commit"
    if target.exists():
        if target.is_symlink() or not stamp.is_file() or stamp.read_text().strip() != spec["commit"]:
            raise RuntimeError(f"Staged source mismatch; run ./sdk clean: {target}")
        return source, target, spec
    target.mkdir(parents=True)
    try:
        with tempfile.TemporaryFile() as archive:
            subprocess.run(["git", "archive", spec["commit"]], cwd=source,
                           stdout=archive, check=True)
            archive.seek(0)
            extract_archive(archive, target)
        shutil.copy2(ROOT / "sdk-support/configs" / f"{name}.config", target / ".config")
        stamp.write_text(spec["commit"] + "\n")
    except BaseException:
        shutil.rmtree(target)
        raise
    return source, target, spec


def build_boot(name, jobs):
    source, target, spec = stage(name)
    if name == "kernel":
        for required in (".config", "vmlinux", "System.map", "arch/arm/boot/zImage"):
            if not (source / required).is_file():
                raise RuntimeError(f"Incomplete protected kernel baseline: {required}")
    env = os.environ.copy()
    env["PATH"] = str(compiler_bin()) + os.pathsep + env.get("PATH", "")
    env["LC_ALL"] = "C"
    env["KBUILD_BUILD_USER"] = "sdk"
    env["KBUILD_BUILD_HOST"] = "f1c200s"
    args = ["make", "ARCH=arm", "CROSS_COMPILE=arm-linux-gnueabi-", "LOCALVERSION="]
    run(args + ["olddefconfig"], target, env)
    if name == "kernel":
        config = (target / ".config").read_text()
        if 'CONFIG_LOCALVERSION=""\n' not in config or "CONFIG_LOCALVERSION_AUTO=y" in config:
            raise RuntimeError("Kernel local version lock violated")
        release = capture(args + ["-s", "kernelrelease"], target, env)
        if release != MANIFEST["kernel_release"]:
            raise RuntimeError(f"Kernel release mismatch: {release}")
    run(args + [f"-j{jobs}"], target, env)
    if name == "kernel":
        run(args + [f"-j{jobs}", "modules"], target, env)
    images = OUT / "images" / name
    images.mkdir(parents=True, exist_ok=True)
    artifacts = [spec["artifact"], ".config"]
    if name == "kernel":
        artifacts += ["vmlinux", "System.map", "Module.symvers"]
    results = {}
    for artifact in artifacts:
        path = target / artifact
        if not path.is_file():
            raise RuntimeError(f"Build did not produce {path}")
        shutil.copy2(path, images / path.name)
        results[path.name] = digest(path)
    report = {"component": name, "source_commit": spec["commit"],
              "compiler": capture([str(compiler_bin() / "arm-linux-gnueabi-gcc"), "--version"]),
              "sha256": results, "board_deployed": False, "cold_boots": 0}
    (images / "build.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


def doctor(release=False):
    errors = []
    for name in MANIFEST["components"]:
        try:
            validate_source(name)
            config = ROOT / "sdk-support/configs" / f"{name}.config"
            if digest(config) != MANIFEST["components"][name]["config_sha256"]:
                raise RuntimeError(f"SDK configuration copy mismatch: {name}")
            print(f"OK source/config: {name}")
        except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
            errors.append(str(error))
    try:
        print(f"OK kernel compiler: {compiler_bin()}")
        check_output()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        errors.append(str(error))
    for command in ("make", "gcc", "bison", "flex", "bc", "openssl", "pkg-config"):
        if not shutil.which(command):
            errors.append(f"Missing host tool: {command}")
    for blocker in MANIFEST["release_blockers"]:
        print(f"PENDING release: {blocker}")
    if release and not MANIFEST["release_ready"]:
        errors.append("Full image is not release-ready; refusing an incomplete SDK image")
    for error in errors:
        print(f"ERROR: {error}", file=sys.stderr)
    return 1 if errors else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command")
    inspect = sub.add_parser("doctor")
    inspect.add_argument("--release", action="store_true")
    build = sub.add_parser("build")
    build.add_argument("component", choices=["all", "kernel", "uboot"], default="all", nargs="?")
    build.add_argument("-j", "--jobs", type=int, default=8)
    sub.add_parser("clean")
    args = parser.parse_args()
    if not args.command:
        parser.error("a command is required")
    with locked():
        if args.command == "doctor":
            return doctor(args.release)
        if args.command == "clean":
            check_output()
            if OUT.exists():
                shutil.rmtree(OUT)
            print("SDK output removed; active source/config/toolchain/downloads preserved")
            return 0
        if args.jobs < 1 or args.jobs > 64:
            raise RuntimeError("jobs must be between 1 and 64")
        if args.component == "all":
            return doctor(release=True)
        build_boot(args.component, args.jobs)
        return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"SDK failed: {error}", file=sys.stderr)
        sys.exit(1)
