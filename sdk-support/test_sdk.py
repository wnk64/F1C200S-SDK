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

spec = importlib.util.spec_from_file_location("f1sdk", str(Path(__file__).with_name("sdk.py")))
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

    def test_clean_removes_only_owned_output(self):
        source = self.root / "source.c"
        source.write_text("preserve")
        sdk.check_output(create=True)
        (sdk.OUT / "generated.o").write_text("generated")
        with mock.patch("sys.argv", ["sdk", "clean"]):
            self.assertEqual(sdk.main(), 0)
        self.assertFalse(sdk.OUT.exists())
        self.assertEqual(source.read_text(), "preserve")

    def test_clean_unknown_directory_does_not_delete(self):
        sdk.OUT.mkdir()
        (sdk.OUT / "source.c").write_text("preserve")
        with mock.patch("sys.argv", ["sdk", "clean"]):
            with self.assertRaises(RuntimeError):
                sdk.main()
        self.assertTrue((sdk.OUT / "source.c").exists())

    def test_scoped_clean_preserves_other_components_and_sources(self):
        source = self.root / "active-source"
        source.mkdir()
        (source / "keep.c").write_text("preserve")
        sdk.check_output(create=True)
        generated = sdk.OUT / "sources" / "example"
        generated.mkdir(parents=True)
        (generated / ".sdk-source-commit").write_text("a" * 40)
        other = sdk.OUT / "sources" / "kernel"
        other.mkdir()
        (other / "zImage").write_text("preserve")
        manifest = {"components": {"example": {"path": "active-source"}}}
        with mock.patch.object(sdk, "MANIFEST", manifest), mock.patch.object(sdk, "capture", return_value=""):
            sdk.clean_component("example")
        self.assertFalse(generated.exists())
        self.assertTrue((other / "zImage").is_file())
        self.assertEqual((source / "keep.c").read_text(), "preserve")

    def test_scoped_clean_rejects_unmarked_component(self):
        (self.root / "active-source").mkdir()
        sdk.check_output(create=True)
        generated = sdk.OUT / "sources" / "example"
        generated.mkdir(parents=True)
        (generated / "source.c").write_text("preserve")
        manifest = {"components": {"example": {"path": "active-source"}}}
        with mock.patch.object(sdk, "MANIFEST", manifest):
            with self.assertRaises(RuntimeError):
                sdk.clean_component("example")
        self.assertTrue((generated / "source.c").is_file())

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
        sdk.OUT.symlink_to(str(external), target_is_directory=True)
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
        (sdk.OUT / sdk.MARKER).symlink_to(str(real))
        with self.assertRaises(RuntimeError):
            sdk.check_output()

    def test_lock_excludes_second_process(self):
        with sdk.locked():
            code = "import fcntl,os,sys; f=os.open(sys.argv[1],os.O_RDONLY); fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)"
            result = subprocess.run(["python3", "-c", code, str(self.root)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
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

    def test_config_updates_remove_duplicate_and_unset_lines(self):
        config = self.root / ".config"
        config.write_text('CONFIG_A="old"\n# CONFIG_B is not set\nCONFIG_A="duplicate"\nCONFIG_C=y\n')
        sdk.set_config(config, {"CONFIG_A": '"new"', "CONFIG_B": "y"})
        self.assertEqual(config.read_text(), 'CONFIG_C=y\nCONFIG_A="new"\nCONFIG_B=y\n')

    def test_project_stage_preserves_toolchain_and_uses_locked_archive(self):
        source = self.root / "LVGL/menu"
        source.mkdir(parents=True)
        (source / "Makefile").write_text("source")
        sdk.check_output(create=True)
        toolchain = sdk.OUT / "work/ROOTFS/output/host"
        toolchain.mkdir(parents=True)
        (toolchain / "preserve").write_text("toolchain")
        manifest = {"project_sources": {"commit": "a" * 40, "paths": ["LVGL/menu"]}}
        def git_capture(args, cwd=None):
            return str(self.root) if "--show-toplevel" in args else ""
        def git_archive(args, cwd=None, stdout=None, check=None):
            with tarfile.open(fileobj=stdout, mode="w") as archive:
                content = b"locked source"
                item = tarfile.TarInfo("LVGL/menu/Makefile")
                item.size = len(content)
                archive.addfile(item, io.BytesIO(content))
        with mock.patch.object(sdk, "MANIFEST", manifest), mock.patch.object(sdk, "capture", side_effect=git_capture), mock.patch.object(sdk.subprocess, "run", side_effect=git_archive):
            work = sdk.stage_project_sources()
        self.assertEqual((work / "LVGL/menu/Makefile").read_text(), "locked source")
        self.assertEqual((toolchain / "preserve").read_text(), "toolchain")
        self.assertEqual((source / "Makefile").read_text(), "source")

    def test_project_stage_rejects_existing_unowned_source(self):
        (self.root / "LVGL/menu").mkdir(parents=True)
        sdk.check_output(create=True)
        generated = sdk.OUT / "work/LVGL/menu"
        generated.mkdir(parents=True)
        (generated / "source.c").write_text("preserve")
        manifest = {"project_sources": {"commit": "a" * 40, "paths": ["LVGL/menu"]}}
        def git_capture(args, cwd=None):
            return str(self.root) if "--show-toplevel" in args else ""
        with mock.patch.object(sdk, "MANIFEST", manifest), mock.patch.object(sdk, "capture", side_effect=git_capture):
            with self.assertRaises(RuntimeError):
                sdk.stage_project_sources()
        self.assertEqual((generated / "source.c").read_text(), "preserve")

    def test_stage_rejects_code_change_without_deleting_previous_build(self):
        source = self.root / "active-source"
        source.mkdir()
        sdk.check_output(create=True)
        target = sdk.OUT / "sources/example"
        target.mkdir(parents=True)
        (target / ".sdk-source-commit").write_text("a" * 40)
        (target / "compiled.o").write_text("preserve")
        spec = {"commit": "b" * 40}
        with mock.patch.object(sdk, "validate_source", return_value=(source, spec)), mock.patch.object(sdk, "capture", return_value="source.c"):
            with self.assertRaises(RuntimeError):
                sdk.stage("example")
        self.assertEqual((target / "compiled.o").read_text(), "preserve")

    def test_stage_refreshes_only_metadata_without_resetting_build(self):
        source = self.root / "active-source"
        source.mkdir()
        sdk.check_output(create=True)
        target = sdk.OUT / "sources/example"
        target.mkdir(parents=True)
        (target / ".sdk-source-commit").write_text("a" * 40)
        (target / "compiled.o").write_text("preserve")
        spec = {"commit": "b" * 40}
        def git_archive(args, cwd=None, stdout=None, check=None):
            with tarfile.open(fileobj=stdout, mode="w") as archive:
                content = b"new record"
                item = tarfile.TarInfo("WORKTREE_CHANGELOG.md")
                item.size = len(content)
                archive.addfile(item, io.BytesIO(content))
        with mock.patch.object(sdk, "validate_source", return_value=(source, spec)), mock.patch.object(sdk, "capture", return_value="WORKTREE_CHANGELOG.md"), mock.patch.object(sdk.subprocess, "run", side_effect=git_archive):
            sdk.stage("example")
        self.assertEqual((target / "compiled.o").read_text(), "preserve")
        self.assertEqual((target / "WORKTREE_CHANGELOG.md").read_text(), "new record")
        self.assertEqual((target / ".sdk-source-commit").read_text().strip(), "b" * 40)


if __name__ == "__main__":
    unittest.main()
