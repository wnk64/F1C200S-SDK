"""Run a loopback TCP-to-WebSocket bridge, preserving native SSH authentication."""
import argparse
import hashlib
import subprocess
from pathlib import Path

root = Path(__file__).parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='wss://y6zhpe96.gd.ddnsto.com/f1-ssh')
parser.add_argument('--port', type=int, default=2222)
args = parser.parse_args()
if not 1024 <= args.port <= 65535:
    parser.error('port must be 1024..65535')
if not args.url.startswith('wss://') and not args.url.startswith('ws://10.0.0.1:18089/'):
    parser.error('Only TLS URLs or the explicit LAN test address are allowed')
binary = root / '_tmp_websocat.exe'
expected = '284339b5b86f2b11496137d97b6705bb2ef27e64f21527c1ba240bdb6177ba3b'
if hashlib.sha256(binary.read_bytes()).hexdigest() != expected:
    raise RuntimeError('Websocat executable hash mismatch')
credentials = root / 'private' / 'basic-auth.txt'
if not credentials.is_file():
    raise RuntimeError('Missing local tunnel credential')
command = [str(binary), '--binary', '--exit-on-eof', '--buffer-size', '65536',
           '--ping-interval', '20', '--ping-timeout', '60',
           '--basic-auth-file', str(credentials),
           'tcp-l:127.0.0.1:%d' % args.port, args.url]
print('Local SSH endpoint: 127.0.0.1:%d; TLS and SSH host verification remain enabled.' % args.port, flush=True)
try:
    raise SystemExit(subprocess.call(command))
except KeyboardInterrupt:
    pass
