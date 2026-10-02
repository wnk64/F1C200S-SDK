"""Export a complete tree from a bundle whose shallow commit parents are absent."""
import argparse
import os
import shutil
import subprocess
import tempfile


def main():
    p = argparse.ArgumentParser()
    p.add_argument('bundle')
    p.add_argument('commit')
    p.add_argument('target')
    args = p.parse_args()
    temporary = tempfile.mkdtemp(prefix='f1-bluealsa-export-', dir='/tmp')
    try:
        repo = os.path.join(temporary, 'objects.git')
        subprocess.check_call(['git', 'init', '--bare', repo])
        with open(args.bundle, 'rb') as bundle:
            if bundle.readline().strip() != b'# v2 git bundle':
                raise RuntimeError('Unsupported bundle format')
            while True:
                line = bundle.readline()
                if not line:
                    raise RuntimeError('Missing bundle pack')
                if line == b'\n':
                    break
            run = subprocess.Popen(['git', '--git-dir=' + repo, 'index-pack', '--stdin'], stdin=subprocess.PIPE)
            try:
                shutil.copyfileobj(bundle, run.stdin)
            finally:
                run.stdin.close()
            if run.wait() != 0:
                raise RuntimeError('Bundle pack import failed')
        subprocess.check_call(['git', '--git-dir=' + repo, 'archive', '--format=tar.gz',
                               '--prefix=bluealsa/', '-o', args.target, args.commit])
        print('Full source tree exported; historical parent closure not implied')
    finally:
        shutil.rmtree(temporary)


if __name__ == '__main__':
    main()
