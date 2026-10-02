"""Capture the current480x800 XRGB8888 framebuffer without board-side files."""
import struct
import subprocess
import zlib
from pathlib import Path

command = [r"D:\AI工具\putty\plink.exe", "-batch", "-ssh", "root@10.0.0.234", "-pw", "1",
           "-hostkey", "SHA256:I1li/9+0wRwOzVnASuXFl8VwY6uZTMTKaE13b84S/Dc",
           "dd if=/dev/fb0 bs=4096 count=375"]
run = subprocess.run(command, capture_output=True, timeout=20)
if run.returncode or len(run.stdout) != 480 * 800 * 4:
    raise RuntimeError("Framebuffer capture failed: " + run.stderr.decode(errors="replace"))
rows = []
for y in range(800):
    raw = run.stdout[y * 1920:(y + 1) * 1920]
    rgb = bytearray(1440)
    rgb[0::3], rgb[1::3], rgb[2::3] = raw[2::4], raw[1::4], raw[0::4]
    rows.append(b'\x00' + rgb)
def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 480, 800, 8, 2, 0, 0, 0))
png += chunk(b'IDAT', zlib.compress(b''.join(rows))) + chunk(b'IEND', b'')
target = Path(__file__).with_name('touch-screen.png')
target.write_bytes(png)
print(str(target))
