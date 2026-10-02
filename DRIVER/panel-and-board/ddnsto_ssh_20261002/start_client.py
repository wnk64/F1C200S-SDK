"""Start the transient verification relay hidden; do not change system policy."""
import socket
import subprocess
import sys
from pathlib import Path

root = Path(__file__).parent
try:
    with socket.create_connection(('127.0.0.1', 2222), 1):
        raise SystemExit('Port 2222 already in use; existing process preserved.')
except OSError:
    pass
with (root / 'client.log').open('ab') as stdout, (root / 'client-error.log').open('ab') as stderr:
    process = subprocess.Popen([sys.executable, str(root / 'client.py'), '--port', '2222'],
                               stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr,
                               creationflags=subprocess.CREATE_NO_WINDOW | subprocess.DETACHED_PROCESS)
(root / 'client.pid').write_text(str(process.pid), encoding='ascii')
print('SSH relay PID=%d; endpoint 127.0.0.1:2222' % process.pid)
