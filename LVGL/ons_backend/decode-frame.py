"""Convert a native 480x800 XRGB8888 framebuffer capture to PNG."""
import struct
import zlib
from pathlib import Path
import sys

source = Path(sys.argv[1])
raw = source.read_bytes()
if len(raw) != 480 * 800 * 4:
    raise ValueError("Expected native 480x800 XRGB8888 framebuffer")
rows = []
for y in range(800):
    row = raw[y * 1920:(y + 1) * 1920]
    rgb = bytearray(1440)
    rgb[0::3], rgb[1::3], rgb[2::3] = row[2::4], row[1::4], row[0::4]
    rows.append(b'\x00' + rgb)

def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)

png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 480, 800, 8, 2, 0, 0, 0))
png += chunk(b'IDAT', zlib.compress(b''.join(rows))) + chunk(b'IEND', b'')
source.with_suffix('.png').write_bytes(png)
