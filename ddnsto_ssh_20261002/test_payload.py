"""Produce a bounded binary SCP payload, including NUL and all byte values."""
from pathlib import Path
import hashlib
target = Path(__file__).with_name('_tmp_payload.bin')
target.write_bytes(bytes(range(256)) * 1024)
print(hashlib.sha256(target.read_bytes()).hexdigest())
