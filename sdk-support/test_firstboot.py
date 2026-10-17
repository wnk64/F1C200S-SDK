#!/usr/bin/env python3
"""Read/write tests use private sparse files, never a host or board disk."""
import importlib.util
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
import os

spec = importlib.util.spec_from_file_location("f1sdk_firstboot", str(Path(__file__).with_name("sdk.py")))
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)


class FirstBootTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="f1-data-test-")
        self.root = Path(self.temporary.name)
        self.binary = self.root / "partition-helper"
        self.support = Path(__file__).with_name("runtime-data")
        subprocess.run(["cc", "-std=gnu99", "-Wall", "-Wextra", "-Werror", "-Os",
                        str(self.support / "f1_data_partition.c"), "-o", str(self.binary)], check=True)
        self.image = self.root / "card.img"
        self.mbr = sdk.sd_mbr(sdk.sd_partition_table(), 1048576, 548864)
        with self.image.open("wb") as output:
            output.write(self.mbr)
            output.seek(8192)
            output.write(b"protected bootloader bytes")
            output.truncate(2 * 1024 ** 3)

    def tearDown(self):
        self.temporary.cleanup()

    def invoke(self, mode, valid=True):
        result = subprocess.run([str(self.binary), "--image", str(self.image), mode],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(result.returncode == 0, valid, result.stderr.decode())
        return result.stdout.decode().strip()

    def modify(self, offset, value):
        with self.image.open("r+b") as output:
            output.seek(offset)
            output.write(value)

    def test_create_resume_and_protect_existing_data(self):
        before = self.image.open("rb").read(16384)
        self.assertEqual(self.invoke("--fresh"), "1048576 3145728")
        self.invoke("--create")
        after = self.image.open("rb").read(16384)
        self.assertEqual(before[:478], after[:478])
        self.assertEqual(before[494:], after[494:])
        self.assertEqual(after[482], 0x0c)
        self.invoke("--fresh", False)
        self.invoke("--resume")
        self.invoke("--create")
        self.assertEqual(self.image.open("rb").read(16384), after)

    def test_reject_foreign_signature_and_system_geometry(self):
        for offset in (440, 450, 454, 466, 470, 510):
            self.modify(0, self.mbr)
            self.modify(offset, b"\x7f")
            before = self.image.open("rb").read(512)
            self.invoke("--create", False)
            self.assertEqual(self.image.open("rb").read(512), before)

    def test_reject_fourth_or_foreign_third_partition(self):
        for offset in (478, 494):
            self.modify(0, self.mbr)
            self.modify(offset + 4, b"\x0c")
            self.invoke("--create", False)
            self.invoke("--resume", False)

    def test_reject_small_nonsector_and_over_mbr_limit(self):
        for size in (512 * 1024 ** 2, 2 * 1024 ** 3 + 1, 2 * 1024 ** 4):
            with self.image.open("r+b") as output:
                output.truncate(size)
            self.invoke("--create", False)

    def test_tail_alignment_and_symlink_refusal(self):
        with self.image.open("r+b") as output:
            output.truncate(2 * 1024 ** 3 + 512)
        self.assertEqual(self.invoke("--fresh"), "1048576 3145728")
        link = self.root / "alias"
        link.symlink_to(self.image)
        result = subprocess.run([str(self.binary), "--image", str(link), "--create"])
        self.assertNotEqual(result.returncode, 0)

    def test_initializer_syntax_intent_sync_and_self_removal_order(self):
        script = self.support / "S18data-firstboot"
        subprocess.run(["sh", "-n", str(script)], check=True)
        source = script.read_text()
        self.assertLess(source.index('"$TOOL" --fresh'), source.index("save_state format"))
        self.assertLess(source.index("save_state format"), source.index('"$TOOL" --create'))
        self.assertLess(source.index("/sbin/fsck.fat -n"), source.index("save_state done"))
        self.assertLess(source.index("save_state done"), source.index('rm -f "$STATE"'))
        self.assertLess(source.index('rm -f "$STATE"'), source.index("/sbin/reboot"))
        self.assertIn('"$saved_cid" = "$cid"', source)
        helper = (self.support / "f1_data_partition.c").read_text()
        self.assertIn('root.st_dev != system.st_rdev', helper)
        mount_source = (self.support.parent / "runtime-cast/S19data").read_text()
        self.assertLess(mount_source.index("/etc/f1-data-firstboot.state"), mount_source.index('mount -t vfat'))

    def run_initializer(self, state="new\n", fresh_ok=True, format_ok=True):
        area = self.root / "sandbox"
        area.mkdir()
        tools = area / "tools"
        tools.mkdir()
        log = area / "commands"
        marker, initializer, helper = area / "state", area / "init", tools / "helper"
        marker.write_text(state)
        partition, data = area / "p3", area / "roms"
        partition.write_text("")
        fixture = area / "sys"
        fixture.mkdir()
        for name, value in (("cid", "aabb"), ("size", "4194304"), ("start", "1048576"), ("p3size", "3145728")):
            (fixture / name).write_text(value + "\n")
        mounts = area / "mounts"
        mounts.write_text("/dev/root / ext4 rw 0 0\n")
        for name in ("sync", "mount", "umount", "mdev", "reboot", "mkfs.fat", "fsck.fat"):
            command = tools / name
            command.write_text("#!/bin/sh\necho " + name + " >> " + str(log) + "\n" +
                               ("exit 1\n" if name == "mkfs.fat" and not format_ok else "exit 0\n"))
            command.chmod(0o755)
        helper.write_text("#!/bin/sh\necho helper:$1 >> " + str(log) + "\n" +
                          ('[ "$1" != --fresh ] || exit 1\n' if not fresh_ok else "") +
                          "echo '1048576 3145728'\n")
        helper.chmod(0o755)
        source = (self.support / "S18data-firstboot").read_text()
        changes = {
            "PATH=/sbin:/usr/sbin:/bin:/usr/bin": "PATH=" + str(tools) + ":/usr/bin:/bin",
            "/etc/f1-data-firstboot.state": str(marker), "/etc/init.d/S18data-firstboot": str(initializer),
            "/usr/sbin/f1-data-partition": str(helper), "/dev/mmcblk0p3": str(partition),
            "/root/roms": str(data), "/proc/mounts": str(mounts),
            "/sys/class/block/mmcblk0/device/cid": str(fixture / "cid"),
            "/sys/class/block/mmcblk0/size": str(fixture / "size"),
            "/sys/class/block/mmcblk0p3/start": str(fixture / "start"),
            "/sys/class/block/mmcblk0p3/size": str(fixture / "p3size"),
            "/sbin/mkfs.fat": str(tools / "mkfs.fat"), "/sbin/fsck.fat": str(tools / "fsck.fat"),
            "/sbin/reboot": str(tools / "reboot"), '[ ! -b "$PART" ]': '[ ! -f "$PART" ]',
            "sleep 10\ndie 'reboot did not complete'": "exit 0",
        }
        for old, new in changes.items():
            source = source.replace(old, new)
        initializer.write_text(source)
        result = subprocess.run(["sh", str(initializer)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return result, marker, initializer, helper, data, log

    def test_new_initializer_formats_removes_and_reboots(self):
        result, marker, script, helper, data, log = self.run_initializer()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertFalse(marker.exists() or script.exists() or helper.exists())
        self.assertTrue((data / "video").is_dir() and (data / "uploads").is_dir())
        self.assertIn("mkfs.fat", log.read_text())
        self.assertTrue(log.read_text().endswith("sync\nreboot\n"))

    def test_interrupted_format_resumes_only_matching_card(self):
        result, marker, script, helper, data, log = self.run_initializer("format aabb 4194304\n")
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertNotIn("helper:--fresh", log.read_text())
        self.assertIn("mkfs.fat", log.read_text())

    def test_completed_initializer_does_not_reformat(self):
        result, marker, script, helper, data, log = self.run_initializer("done aabb 4194304\n")
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertNotIn("mkfs.fat", log.read_text())

    def test_foreign_card_retains_script_without_format_or_reboot(self):
        result, marker, script, helper, data, log = self.run_initializer("format ccdd 4194304\n")
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(marker.exists() and script.exists() and helper.exists())
        self.assertFalse(log.exists())

    def test_existing_partition_retains_script_without_format(self):
        result, marker, script, helper, data, log = self.run_initializer(fresh_ok=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(marker.read_text(), "new\n")
        self.assertNotIn("mkfs.fat", log.read_text())
        self.assertNotIn("reboot", log.read_text())

    def test_failed_format_retains_durable_intent_without_reboot(self):
        result, marker, script, helper, data, log = self.run_initializer(format_ok=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(marker.read_text(), "format aabb 4194304\n")
        self.assertTrue(script.exists() and helper.exists())
        self.assertNotIn("reboot", log.read_text())


if __name__ == "__main__":
    unittest.main()
