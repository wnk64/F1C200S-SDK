#!/usr/bin/env python3
"""Read/write tests use private sparse files, never a host or board disk."""
import importlib.util
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

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
        self.assertIn('"$rootid" = "$partid"', source)


if __name__ == "__main__":
    unittest.main()
