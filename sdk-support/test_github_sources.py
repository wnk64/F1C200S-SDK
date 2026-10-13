import importlib.util
import io
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location('sources', str(Path(__file__).with_name('github_sources.py')))
sources = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sources)


class SourceRestoreTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.base = Path(self.temporary.name)
        self.root = self.base / 'sdk'
        self.root.mkdir()
        self.archive = self.base / 'dependencies.tar.gz'

    def tearDown(self):
        self.temporary.cleanup()

    def make_archive(self, filename='TOOLCHAIN/file', data=b'source', link=None):
        with tarfile.open(str(self.archive), 'w:gz') as tree:
            item = tarfile.TarInfo(filename)
            if link:
                item.type = tarfile.SYMTYPE
                item.linkname = link
                tree.addfile(item)
            else:
                item.size = len(data)
                tree.addfile(item, io.BytesIO(data))
        return sources.digest(self.archive)

    def test_asset_extract_is_idempotent(self):
        checksum = self.make_archive()
        for unused in range(2):
            sources.extract_dependencies(self.root, self.archive, checksum)
        self.assertEqual((self.root / 'TOOLCHAIN/file').read_bytes(), b'source')

    def test_asset_rejects_checksum_mismatch_before_writes(self):
        self.make_archive()
        with self.assertRaises(RuntimeError):
            sources.extract_dependencies(self.root, self.archive, 'wrong')
        self.assertEqual(list(self.root.iterdir()), [])

    def test_asset_does_not_overwrite_different_existing_files(self):
        checksum = self.make_archive()
        destination = self.root / 'TOOLCHAIN/file'
        destination.parent.mkdir()
        destination.write_bytes(b'keep')
        with self.assertRaises(RuntimeError):
            sources.extract_dependencies(self.root, self.archive, checksum)
        self.assertEqual(destination.read_bytes(), b'keep')

    def test_path_traversal_rejected(self):
        checksum = self.make_archive('../escape')
        with self.assertRaises(RuntimeError):
            sources.extract_dependencies(self.root, self.archive, checksum)
        self.assertFalse((self.base / 'escape').exists())

    def test_external_symlink_rejected(self):
        checksum = self.make_archive('TOOLCHAIN/link', link='../../escape')
        with self.assertRaises(RuntimeError):
            sources.extract_dependencies(self.root, self.archive, checksum)

    def test_existing_source_symlink_rejected(self):
        (self.root / 'KERNEL').symlink_to(self.base)
        with self.assertRaises(RuntimeError):
            sources.component_path(self.root, 'KERNEL')

    def test_existing_repository_is_not_recloned(self):
        (self.root / 'KERNEL').mkdir()
        with mock.patch.object(sources, 'verify_repository') as verify, mock.patch.object(sources, 'run') as run:
            sources.restore_sources(self.root, {'repositories': {'KERNEL': {}}}, 'remote')
            verify.assert_called_once_with(self.root, 'KERNEL', {})
            run.assert_not_called()

    def test_unexpected_head_is_not_overwritten(self):
        (self.root / 'KERNEL').mkdir()
        with mock.patch.object(sources, 'capture', side_effect=[str(self.root / 'KERNEL'), 'wrong']):
            with self.assertRaises(RuntimeError):
                sources.verify_repository(self.root, 'KERNEL', {'head': 'expected', 'commits': []})

    def test_new_clone_checks_out_lock_not_publication_branch_tip(self):
        item = {'branch': 'source/wiliwili', 'head': 'locked', 'commits': ['locked']}
        with mock.patch.object(sources, 'verify_repository') as verify, mock.patch.object(sources, 'run') as run, mock.patch.object(sources.subprocess, 'call', return_value=0):
            sources.restore_sources(self.root, {'repositories': {'LVGL/wiliwili': item}}, 'remote')
            self.assertEqual(run.call_args_list[-1], mock.call(['git', 'checkout', '--detach', 'locked'], self.root / 'LVGL/wiliwili'))
            verify.assert_called_once_with(self.root, 'LVGL/wiliwili', item)

    def test_baseline_checksums_verified(self):
        path = self.root / 'baseline'
        path.write_bytes(b'baseline')
        data = {'baseline': [{'path': 'baseline', 'sha256': sources.digest(path)}]}
        sources.verify_baseline(self.root, data)
        path.write_bytes(b'changed')
        with self.assertRaises(RuntimeError):
            sources.verify_baseline(self.root, data)


if __name__ == '__main__':
    unittest.main()
