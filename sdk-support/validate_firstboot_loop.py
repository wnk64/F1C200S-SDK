#!/usr/bin/env python3
"""Root-only integration test on a newly allocated loop-backed disposable file."""
import importlib.util
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def run(args, **kwargs):
    subprocess.run([str(arg) for arg in args], check=True, **kwargs)


def install(root, binary, destination):
    target = root / destination.lstrip("/")
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(str(binary), str(target))
    dependency_result = subprocess.run(["ldd", str(binary)], stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE, universal_newlines=True)
    deps = dependency_result.stdout
    if dependency_result.returncode and "not a dynamic executable" not in deps + dependency_result.stderr:
        raise RuntimeError("cannot inspect binary dependencies: " + str(binary))
    for dependency in re.findall(r"(/[^\s()]+)", deps):
        source = Path(dependency)
        if source.is_file():
            dest = root / dependency.lstrip("/")
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(str(source), str(dest))


def main():
    if os.geteuid() != 0:
        raise SystemExit("run with sudo; only a new temporary loop file is modified")
    spec = importlib.util.spec_from_file_location("sdk_loop_test", str(ROOT / "sdk-support/sdk.py"))
    sdk = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(sdk)
    with tempfile.TemporaryDirectory(prefix="f1-firstboot-loop-") as temporary:
        area = Path(temporary)
        image = area / "disposable.img"
        mbr = sdk.sd_mbr(sdk.sd_partition_table(), 1048576, 548864)
        with image.open("wb") as output:
            output.write(mbr)
            output.seek(8192)
            output.write(b"preserve SPL marker")
            output.truncate(2 * 1024 ** 3)
        loop = subprocess.check_output(["losetup", "--find", "--show", "--partscan", str(image)],
                                       universal_newlines=True).strip()
        if not re.fullmatch(r"/dev/loop[0-9]+", loop):
            raise RuntimeError("unexpected loop path")
        backing = subprocess.check_output(["losetup", "-n", "-O", "BACK-FILE", loop], universal_newlines=True).strip()
        if Path(backing).resolve() != image.resolve():
            raise RuntimeError("loop does not belong to the test")
        root = area / "root"
        root.mkdir()
        mounted = False
        try:
            run(["mkfs.ext4", "-q", "-F", loop + "p2"])
            run(["mount", loop + "p2", root])
            mounted = True
            install(root, "/bin/busybox", "/bin/busybox")
            (root / "sbin").mkdir(exist_ok=True)
            for name in ("sh", "awk", "cat", "mkdir", "mv", "rm", "sync", "mount", "umount", "mdev"):
                (root / "bin" / name).symlink_to("busybox")
            # A fake reboot only records the request. No host reboot is possible.
            reboot = root / "sbin/reboot"
            reboot.write_text("#!/bin/sh\necho requested > /reboot.requested\n")
            reboot.chmod(0o755)
            sleep = root / "bin/sleep"
            sleep.write_text("#!/bin/sh\nexit 0\n")
            sleep.chmod(0o755)
            install(root, "/sbin/mkfs.vfat", "/sbin/mkfs.fat")
            install(root, "/sbin/fsck.vfat", "/sbin/fsck.fat")
            install(root, ROOT / ".sdk-output/runtime-tools-build/f1-data-partition.host", "/usr/sbin/f1-data-partition")
            (root / "etc/init.d").mkdir(parents=True)
            support = ROOT / "sdk-support/runtime-data"
            script = root / "etc/init.d/S18data-firstboot"
            shutil.copy2(str(support / script.name), str(script))
            state = root / "etc/f1-data-firstboot.state"
            shutil.copy2(str(support / state.name), str(state))
            (root / "proc").mkdir()
            (root / "proc/mounts").write_text("/dev/root / ext4 rw 0 0\n")
            (root / "dev").mkdir()
            disk = os.stat(loop)
            system = os.stat(loop + "p2")
            for name, device in (("mmcblk0", disk.st_rdev), ("mmcblk0p2", system.st_rdev),
                                 ("mmcblk0p3", os.makedev(os.major(system.st_rdev), os.minor(system.st_rdev) + 1))):
                os.mknod(str(root / "dev" / name), stat.S_IFBLK | 0o600, device)
            os.mknod(str(root / "dev/null"), stat.S_IFCHR | 0o666, os.makedev(1, 3))
            values = {"mmcblk0/device/cid": "aabbccdd", "mmcblk0/size": "4194304",
                      "mmcblk0p3/start": "1048576", "mmcblk0p3/size": "3145728"}
            for name, value in values.items():
                path = root / "sys/class/block" / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(value + "\n")
            result = subprocess.run(["chroot", str(root), "/bin/sh", "/etc/init.d/S18data-firstboot"])
            if result.returncode != 1 or not (root / "reboot.requested").is_file():
                raise RuntimeError("expected fake reboot then deliberate reboot-did-not-complete guard")
            if script.exists() or state.exists() or (root / "usr/sbin/f1-data-partition").exists():
                raise RuntimeError("initializer did not remove itself")
            if Path("/sys/class/block/" + Path(loop).name + "p3/size").read_text().strip() != "3145728":
                raise RuntimeError("BLKPG registration mismatch")
            run(["fsck.vfat", "-n", loop + "p3"])
            listing = subprocess.check_output(["mdir", "-i", loop + "p3", "::"],
                                              env=dict(os.environ, MTOOLS_SKIP_CHECK="1"), universal_newlines=True)
            if "uploads" not in listing or "music" not in listing:
                raise RuntimeError("data directories missing")
            after = image.open("rb").read(16384)
            if after[:478] != mbr[:478] or after[494:512] != mbr[494:512] or after[8192:8211] != b"preserve SPL marker":
                raise RuntimeError("protected disk bytes changed")
            print("PASS real BLKPG p3 creation on mounted root, FAT32/fsck/directories/self-removal/fake reboot; system entries/SPL unchanged")
        finally:
            # Cleanup only the exact mountpoints/loop allocated above.
            subprocess.run(["umount", str(root / "root/roms")], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if mounted:
                run(["umount", root])
            run(["losetup", "-d", loop])


if __name__ == "__main__":
    main()
