"""Archive exact source refs and build dependencies without changing worktrees.

Run on the existing Ubuntu VM with Python3.5+. Does not deploy or publish.
"""
import hashlib
import json
import os
import shutil
import subprocess
import tarfile

DEST = '/home/wnk/F1C200S_archives/miracast_snapshot_20261002'
NANO = '/home/wnk/LicheePi_Nano'
REPOS = {
    'kernel': (NANO + '/linux_musb_clean_ep1_20260811', 'b3d8ed3'),
    'aic8800': ('/home/wnk/aic8800_ugreen_v14_20260919', '53bec9e'),
    'player': ('/home/wnk/f1c200s_display_480x800_candidate_20260914', '5685c63'),
    'sink': (NANO + '/third_party/lazycast_host_20260721', 'HEAD'),
}


def git(path, *args):
    return subprocess.check_output(['git', '-C', path] + list(args)).decode().strip()


def digest(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def pack(name, paths, exclude=None):
    target = os.path.join(DEST, name)
    def select(info):
        parts = info.name.split('/')
        if '.git' in parts or '__pycache__' in parts:
            return None
        if exclude and exclude(info.name):
            return None
        return info
    with tarfile.open(target, 'w:gz') as archive:
        for source, member in paths:
            archive.add(source, arcname=member, filter=select)
    print('ARCHIVED ' + name, flush=True)


def main():
    if os.path.exists(DEST):
        raise RuntimeError('Archive already exists; refusing overwrite')
    shutil.copytree(NANO + '/third_party/lazycast_host_20260721/runtime/snapshot-20261002', DEST)
    os.makedirs(os.path.join(DEST, 'sources'))
    os.makedirs(os.path.join(DEST, 'bundles'))
    manifest = {'status': 'successful-runtime-snapshot; three-cold pending',
                'github': 'git@github.com:wnk64/licheepi_nano.git', 'sources': {}}
    for name, (path, ref) in REPOS.items():
        commit = git(path, 'rev-parse', ref)
        manifest['sources'][name] = {'path': path, 'commit': commit,
                                    'tracked_files': len(git(path, 'ls-tree', '-r', '--name-only', commit).splitlines())}
        target = os.path.join(DEST, 'sources', name + '.tar.gz')
        subprocess.check_call(['git', '-C', path, 'archive', '--format=tar.gz',
                               '--prefix=' + name + '/', '-o', target, commit])
        subprocess.check_call(['git', '-C', path, 'bundle', 'create',
                               os.path.join(DEST, 'bundles', name + '.bundle'), '--all'])
        subprocess.check_call(['git', '-C', path, 'bundle', 'verify',
                               os.path.join(DEST, 'bundles', name + '.bundle')])
        print('SOURCE ' + name + ' ' + commit, flush=True)
    pack('kernel-build-state.tar.gz', [(NANO + '/linux_musb_clean_ep1_20260811/' + f, f)
         for f in ['.config', 'vmlinux', 'System.map', 'Module.symvers',
                   'arch/arm/boot/zImage', 'arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dtb',
                   'include/generated', 'arch/arm/include/generated']] +
         [('/home/wnk/F1C200S_archives/fbdev_single_20261001/candidate', 'baseline-build-artifacts')])
    shutil.copy2('/home/wnk/aic8800_local_baselines/ugreen_53bec9e_20260928/build-and-kernel-prereq.tar',
                 os.path.join(DEST, 'aic-build-prerequisites.tar'))
    pack('build-sdk.tar.gz', [(NANO + '/buildroot-2018.02.11/output/host', 'buildroot/output/host'),
         (NANO + '/buildroot-2018.02.11/output/target/usr/lib', 'buildroot/output/target/usr/lib'),
         ('/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi', 'gcc-linaro-7.2.1')])
    pack('sources/libcedarx.tar.gz', [(NANO + '/buildroot-2018.02.11/output/build/libcedarx-custom', 'libcedarx')])
    pack('sources/protocol-tools.tar.gz', [(NANO + '/aic_pbc_rearm_20260818/sources', 'protocol-tools')])
    pack('buildroot-source-and-config.tar.gz', [(NANO + '/buildroot-2018.02.11', 'buildroot')],
         lambda name: name.startswith('buildroot/output/') or name.startswith('buildroot/dl/'))
    runtime = os.path.join(DEST, 'runtime.tar')
    with tarfile.open(runtime) as archive:
        for info in archive:
            if info.name.startswith('/') or '..' in info.name.split('/'):
                raise RuntimeError('Unsafe runtime member: ' + info.name)
            if 'rtl8723bs-sta.conf' in info.name or info.name.endswith(('.key', 'id_rsa')):
                raise RuntimeError('Sensitive runtime member: ' + info.name)
    manifest['files'] = {}
    for root, dirs, files in os.walk(DEST):
        for name in files:
            path = os.path.join(root, name)
            relative = os.path.relpath(path, DEST)
            manifest['files'][relative] = {'sha256': digest(path), 'bytes': os.path.getsize(path)}
    with open(os.path.join(DEST, 'manifest.json'), 'w') as f:
        json.dump(manifest, f, indent=2, sort_keys=True)
    with open(os.path.join(DEST, 'SHA256SUMS'), 'w') as f:
        for name, entry in sorted(manifest['files'].items()):
            f.write(entry['sha256'] + '  ' + name + '\n')
        f.write(digest(os.path.join(DEST, 'manifest.json')) + '  manifest.json\n')
    print('ARCHIVE COMPLETE ' + DEST, flush=True)


if __name__ == '__main__':
    main()
