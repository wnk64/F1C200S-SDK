"""Finalize and audit the archive, then stage bounded public recovery inputs."""
import hashlib
import io
import json
import os
import re
import shutil
import subprocess
import tarfile

ROOT = '/home/wnk/F1C200S_archives/miracast_snapshot_20261002'
REPO = '/home/wnk/LicheePi_Nano/third_party/lazycast_host_20260721'
PUBLIC = REPO + '/runtime/snapshot-20261002'


def sha(path):
    result = hashlib.sha256()
    with open(path, 'rb') as stream:
        for data in iter(lambda: stream.read(1048576), b''):
            result.update(data)
    return result.hexdigest()


def main():
    with open('/tmp/f1-source-archive.rc') as stream:
        failed = stream.read().strip() != '0'
    if failed:
        with open('/tmp/f1-source-archive.log') as stream:
            log = stream.read()
        if 'UnicodeEncodeError' not in log or "f.write(entry['sha256']" not in log:
            raise RuntimeError('Unexpected archive failure; refusing recovery')
        print('Recovering SHA256 list after GBK filename normalization', flush=True)
    for text, replacement in [('\u53d8\u66f4\u8bb0\u5f55.md', 'archive-changelog-cn.md'),
                              ('\u6295\u5c4f\u4f18\u5316\u8bb0\u5f55.txt', 'optimization-record-cn.txt')]:
        old = os.fsencode(ROOT) + b'/' + text.encode('gbk')
        if os.path.exists(old):
            os.rename(old, os.fsencode(ROOT + '/' + replacement))
    with open(ROOT + '/manifest.json') as stream:
        manifest = json.load(stream)
    redacted = []
    source_path = ROOT + '/buildroot-source-and-config.tar.gz'
    with tarfile.open(source_path) as source:
        with tarfile.open(source_path + '.new', 'w:gz') as target:
            for info in source:
                data = source.extractfile(info).read() if info.isfile() else None
                if data is not None and ((b'wnk641' in data and b'psk=' in data)
                                         or info.name.endswith('/rtl8723bs-sta.conf')):
                    redacted.append(info.name)
                    data = b'# Private network credentials excluded from public archive.\nnetwork={\n    ssid="YOUR_SSID"\n    psk="YOUR_PASSWORD"\n}\n'
                    info.size = len(data)
                target.addfile(info, io.BytesIO(data) if data is not None else None)
    os.replace(source_path + '.new', source_path)
    manifest['private_network_templates'] = redacted or manifest.get('private_network_templates', [])
    # Source7a0a64a is the exact executable source before archival metadata.
    subprocess.check_call(['git', '-C', REPO, 'archive', '--format=tar.gz',
        '--prefix=sink/', '-o', ROOT + '/sources/sink.tar.gz', '7a0a64a'])
    manifest['sources']['sink']['commit'] = subprocess.check_output(
        ['git', '-C', REPO, 'rev-parse', '7a0a64a']).decode().strip()
    manifest['sources']['sink']['tracked_files'] = len(subprocess.check_output(
        ['git', '-C', REPO, 'ls-tree', '-r', '--name-only', '7a0a64a']).splitlines())
    for name, path, ref in [('panel', '/home/wnk/SoftWare/Driver software/tk032f8004-panel', 'HEAD'),
                            ('rtl-support', '/home/wnk/LicheePi_Nano/third_party/rtl8723ds', 'ca485c2')]:
        manifest['sources'][name] = {'path': path, 'commit': subprocess.check_output(
            ['git', '-C', path, 'rev-parse', ref]).decode().strip()}
    manifest['sources']['bluealsa'] = {'commit': '82fdaea18130bf7b0c83a5e2566ddd4249901c82',
                                      'source': 'sources/bluealsa.tar.gz',
                                      'history': 'shallow bundle parents incomplete; full commit tree exported'}
    # Full local state includes the historical source bundle; publish a compact
    # recovery state instead, with source history available on the kernel ref.
    with tarfile.open(ROOT + '/kernel-build-state.tar.gz') as source:
        with tarfile.open(ROOT + '/kernel-recovery-artifacts.tar.gz', 'w:gz') as target:
            for info in source:
                if info.name.endswith('source.bundle') or info.name.endswith('generated-prerequisites.tar'):
                    continue
                target.addfile(info, source.extractfile(info) if info.isfile() else None)
    for name in ['README.md', 'RESULTS.md', 'archive_snapshot.py', 'finalize_snapshot.py', 'export_bundle_tree.py',
                 'start-cast.sh', 'WORKTREE_CHANGELOG.md', 'archive-changelog-cn.md', 'optimization-record-cn.txt']:
        shutil.copy2(PUBLIC + '/' + name, ROOT + '/' + name)
    manifest['files'] = {}
    for directory, dirs, files in os.walk(ROOT):
        dirs[:] = [d for d in dirs if d != '__pycache__']
        for name in files:
            if directory == ROOT and name in ['manifest.json', 'SHA256SUMS']:
                continue
            path = directory + '/' + name
            relative = os.path.relpath(path, ROOT)
            manifest['files'][relative] = {'sha256': sha(path), 'bytes': os.path.getsize(path)}
    with open(ROOT + '/manifest.json', 'w') as stream:
        json.dump(manifest, stream, indent=2, sort_keys=True)
    with open(ROOT + '/SHA256SUMS', 'w') as stream:
        for name, values in sorted(manifest['files'].items()):
            stream.write(values['sha256'] + '  ' + name + '\n')
        stream.write(sha(ROOT + '/manifest.json') + '  manifest.json\n')
    publish = ['manifest.json', 'SHA256SUMS', 'aic-build-prerequisites.tar',
               'kernel-recovery-artifacts.tar.gz', 'buildroot-source-and-config.tar.gz',
               'sources/aic8800.tar.gz', 'sources/player.tar.gz', 'sources/sink.tar.gz',
               'sources/libcedarx.tar.gz', 'sources/protocol-tools.tar.gz',
               'sources/panel.tar.gz', 'sources/rtl-support.tar.gz', 'sources/bluealsa.tar.gz',
               'sources/sbc-1.3.tar.xz', 'support-runtime.tar', 'bluealsa-build-state.tar.gz']
    for name in publish:
        path = ROOT + '/' + name
        if os.path.getsize(path) >= 95 * 1024 * 1024:
            raise RuntimeError('Oversized Git blob: ' + name)
        os.makedirs(os.path.dirname(PUBLIC + '/' + name), exist_ok=True)
        shutil.copy2(path, PUBLIC + '/' + name)
    with open(PUBLIC + '/GITHUB_SHA256SUMS', 'w') as stream:
        for name, values in sorted(manifest['files'].items()):
            if os.path.isfile(PUBLIC + '/' + name):
                if sha(PUBLIC + '/' + name) != values['sha256']:
                    raise RuntimeError('Public archive differs from local: ' + name)
                stream.write(values['sha256'] + '  ' + name + '\n')
        stream.write(sha(PUBLIC + '/manifest.json') + '  manifest.json\n')
    for name in ['sources/kernel.tar.gz', 'sources/aic8800.tar.gz', 'sources/player.tar.gz',
                 'sources/sink.tar.gz', 'sources/libcedarx.tar.gz', 'sources/protocol-tools.tar.gz',
                 'buildroot-source-and-config.tar.gz', 'runtime.tar',
                 'sources/panel.tar.gz', 'sources/rtl-support.tar.gz', 'sources/bluealsa.tar.gz',
                 'sources/sbc-1.3.tar.xz', 'support-runtime.tar']:
        with tarfile.open(ROOT + '/' + name) as archive:
            members = archive.getmembers()
            for info in members:
                if info.name.startswith('/') or '..' in info.name.split('/'):
                    raise RuntimeError('Unsafe member in ' + name)
                lowered = info.name.lower()
                if any(part in lowered for part in ['id_rsa', 'id_ed25519', '.git/']) or lowered.endswith('/rtl8723bs-sta.conf'):
                    raise RuntimeError('Private/unwanted member in ' + name + ':' + info.name)
                if info.isfile() and info.size < 2 * 1024 * 1024:
                    data = archive.extractfile(info).read()
                    if (b'wnk641' in data and b'psk=' in data) or b'BEGIN OPENSSH PRIVATE KEY' in data or re.search(rb'\bghp_[A-Za-z0-9]{30,}\b', data):
                        raise RuntimeError('Credential review required for ' + name + ':' + info.name)
            print('AUDIT {} members={}'.format(name, len(members)), flush=True)
    subprocess.check_call(['sha256sum', '-c', 'SHA256SUMS'], cwd=ROOT)
    print('FINALIZED AND CHECKED ' + ROOT, flush=True)


if __name__ == '__main__':
    main()
