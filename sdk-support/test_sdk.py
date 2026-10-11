#!/usr/bin/env python3
import importlib.util
import io
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location("f1sdk", Path(__file__).with_name("sdk.py"))
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)


class OutputSafetyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name).resolve()
        self.patch = mock.patch.multiple(sdk, ROOT=self.root, OUT=self.root / ".sdk-output")
        self.patch.start()

    def tearDown(self):
        self.patch.stop()
        self.temp.cleanup()

    def test_missing_output_is_noop(self):
        sdk.check_output()
        self.assertFalse(sdk.OUT.exists())

    def test_owned_output_is_allowed(self):
        sdk.check_output(create=True)
        sdk.check_output()
        self.assertEqual((sdk.OUT / sdk.MARKER).read_text(), sdk.TOKEN)

    def test_unknown_directory_is_preserved(self):
        sdk.OUT.mkdir()
        sentinel = sdk.OUT / "source.c"
        sentinel.write_text("user data")
        with self.assertRaises(RuntimeError):
            sdk.check_output()
        self.assertTrue(sentinel.exists())

    def test_symlink_output_is_rejected(self):
        external = self.root / "active-source"
        external.mkdir()
        sdk.OUT.symlink_to(external, target_is_directory=True)
        with self.assertRaises(RuntimeError):
            sdk.check_output(create=True)
        self.assertTrue(external.is_dir())

    def test_bad_marker_is_rejected(self):
        sdk.OUT.mkdir()
        (sdk.OUT / sdk.MARKER).write_text("not owned")
        with self.assertRaises(RuntimeError):
            sdk.check_output()

    def test_symlink_marker_is_rejected(self):
        sdk.OUT.mkdir()
        real = self.root / "marker"
        real.write_text(sdk.TOKEN)
        (sdk.OUT / sdk.MARKER).symlink_to(real)
        with self.assertRaises(RuntimeError):
            sdk.check_output()

    def test_lock_excludes_second_process(self):
        with sdk.locked():
            code = "import fcntl,os,sys; f=os.open(sys.argv[1],os.O_RDONLY); fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)"
            result = subprocess.run(["python3", "-c", code, str(self.root)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)

    def archive(self, member):
        buffer = io.BytesIO()
        with tarfile.open(fileobj=buffer, mode="w") as archive:
            archive.addfile(member)
        buffer.seek(0)
        return buffer

    def test_archive_parent_escape_is_rejected(self):
        target = self.root / "stage"
        target.mkdir()
        with self.assertRaises(RuntimeError):
            sdk.extract_archive(self.archive(tarfile.TarInfo("../escaped")), target)
        self.assertFalse((self.root / "escaped").exists())

    def test_archive_symlink_escape_is_rejected(self):
        member = tarfile.TarInfo("evil")
        member.type = tarfile.SYMTYPE
        member.linkname = "/etc"
        target = self.root / "stage"
        target.mkdir()
        with self.assertRaises(RuntimeError):
            sdk.extract_archive(self.archive(member), target)

    def test_normal_archive_is_extracted(self):
        member = tarfile.TarInfo("source.c")
        target = self.root / "stage"
        target.mkdir()
        sdk.extract_archive(self.archive(member), target)
        self.assertTrue((target / "source.c").is_file())


if __name__ == "__main__":
    unittest.main()
