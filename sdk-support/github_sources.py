#!/usr/bin/env python3
"""Restore source-locked component repositories from one consolidated remote."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tarfile


def run(arguments, cwd=None):
    subprocess.run(arguments, cwd=str(cwd) if cwd else None, check=True)


def capture(arguments, cwd):
    return subprocess.check_output(arguments, cwd=str(cwd), universal_newlines=True).strip()


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def inside(path, root):
    return Path(os.path.realpath(str(root))) in Path(os.path.realpath(str(path))).parents


def component_path(root, relative):
    path = root / relative
    if not inside(path, root) or path.is_symlink():
        raise RuntimeError('Unsafe component path: ' + relative)
    return path


def verify_repository(root, relative, item):
    path = component_path(root, relative)
    if capture(['git', 'rev-parse', '--show-toplevel'], path) != str(path.resolve()):
        raise RuntimeError('Missing independent source repository: ' + relative)
    if capture(['git', 'rev-parse', 'HEAD'], path) != item['head']:
        raise RuntimeError('Unexpected source HEAD; not overwriting: ' + relative)
    for commit in item['commits']:
        run(['git', 'cat-file', '-e', commit + '^{commit}'], path)


def restore_sources(root, publication, remote):
    for relative, item in sorted(publication['repositories'].items()):
        path = component_path(root, relative)
        if path.exists():
            verify_repository(root, relative, item)
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        run(['git', 'clone', '--single-branch', '--no-tags', '--branch', item['branch'], remote, str(path)])
        missing = []
        for commit in item['commits']:
            if subprocess.call(['git', 'cat-file', '-e', commit + '^{commit}'], cwd=str(path),
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL):
                missing.append('refs/tags/sdk-lock/' + commit + ':refs/sdk-lock/' + commit)
        if missing:
            run(['git', 'fetch', '--no-tags', 'origin'] + missing, path)
        verify_repository(root, relative, item)


def extract_dependencies(root, archive, expected):
    if digest(archive) != expected:
        raise RuntimeError('Dependency asset checksum mismatch')
    with tarfile.open(str(archive), 'r:gz') as tree:
        members = tree.getmembers()
        for member in members:
            path = root / member.name
            if not inside(path, root):
                raise RuntimeError('Unsafe dependency path: ' + member.name)
            if not (member.isfile() or member.isdir() or member.issym() or member.islnk()):
                raise RuntimeError('Unsupported dependency member: ' + member.name)
            if member.issym() and not inside(path.parent / member.linkname, root):
                raise RuntimeError('External dependency symlink')
            if member.islnk() and not inside(root / member.linkname, root):
                raise RuntimeError('External dependency hardlink')
            if path.exists() or path.is_symlink():
                if member.isdir() and path.is_dir() and not path.is_symlink():
                    continue
                if member.issym() and path.is_symlink() and os.readlink(str(path)) == member.linkname:
                    continue
                if member.isfile() and path.is_file() and not path.is_symlink():
                    stream = tree.extractfile(member)
                    checksum = hashlib.sha256()
                    for block in iter(lambda: stream.read(1024 * 1024), b''):
                        checksum.update(block)
                    if checksum.hexdigest() == digest(path):
                        continue
                if member.islnk() and path.is_file() and not path.is_symlink():
                    original = root / member.linkname
                    if original.is_file() and digest(path) == digest(original):
                        continue
                raise RuntimeError('Refusing different existing dependency: ' + member.name)
        for member in members:
            path = root / member.name
            if not path.exists() and not path.is_symlink():
                tree.extract(member, str(root))


def verify_baseline(root, publication):
    for item in publication['baseline']:
        path = component_path(root, item['path'])
        if not path.is_file() or digest(path) != item['sha256']:
            raise RuntimeError('Baseline checksum mismatch: ' + item['path'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dependencies', type=Path)
    parser.add_argument('--repository')
    parser.add_argument('--verify-only', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    publication = json.loads((root / 'sdk-support/github-sources.json').read_text())
    if args.verify_only:
        for relative, item in publication['repositories'].items():
            verify_repository(root, relative, item)
        verify_baseline(root, publication)
        print('Source commits and baseline material verified; no build performed')
        return
    if not args.dependencies or not args.dependencies.is_file():
        parser.error('--dependencies must name the downloaded release asset')
    # Validate before any checkout or extraction.
    if digest(args.dependencies) != publication['dependencies']['sha256']:
        raise RuntimeError('Dependency asset checksum mismatch')
    restore_sources(root, publication, args.repository or publication['repository'])
    extract_dependencies(root, args.dependencies, publication['dependencies']['sha256'])
    verify_baseline(root, publication)
    print('SDK sources restored; run ./sdk doctor, then ./sdk build -j8')


if __name__ == '__main__':
    main()
