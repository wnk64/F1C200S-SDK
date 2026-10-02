"""Rasterize the bundled open Takao font into fixed-size on-demand glyph records."""
import ctypes as c
import struct
from pathlib import Path
import generate_menu_font as base

root = Path(__file__).parent
gdi = base.gdi
gdi.AddFontResourceExW.argtypes = [c.c_wchar_p, c.c_uint, c.c_void_p]
gdi.RemoveFontResourceExW.argtypes = [c.c_wchar_p, c.c_uint, c.c_void_p]
font_path = str(root / 'TakaoPGothic.ttf')
if not gdi.AddFontResourceExW(font_path, 0x10, None):
    raise OSError('Private Takao font load failed')
dc = gdi.CreateCompatibleDC(None)
font = gdi.CreateFontW(-28, 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 4, 0, 'TakaoPGothic')
old = gdi.SelectObject(dc, font)
matrix = base.Matrix(base.Fixed(0, 1), base.Fixed(0, 0), base.Fixed(0, 0), base.Fixed(0, 1))
target = root / 'cjk28.bin'
try:
    with target.open('wb') as output:
        output.write(b'F1CJK28\0' + struct.pack('<HHI', 0x4e00, 0xa000, 528))
        for code in range(0x4e00, 0xa000):
            metrics = base.Metrics()
            size = gdi.GetGlyphOutlineW(dc, code, 6, c.byref(metrics), 0, None, c.byref(matrix))
            if size == 0xffffffff or not size or metrics.width * metrics.height > 1024:
                output.write(bytes(528))
                continue
            buffer = c.create_string_buffer(size)
            gdi.GetGlyphOutlineW(dc, code, 6, c.byref(metrics), size, buffer, c.byref(matrix))
            raw = buffer.raw
            stride = (metrics.width + 3) & ~3
            pixels = [(raw[y * stride + x] * 15 + 32) // 64
                      for y in range(metrics.height) for x in range(metrics.width)]
            if len(pixels) % 2:
                pixels.append(0)
            bitmap = bytes((pixels[i] << 4) | pixels[i + 1] for i in range(0, len(pixels), 2))
            header = struct.pack('<HHhhHH', metrics.width, metrics.height, metrics.origin.x,
                                 metrics.origin.y - metrics.height, metrics.advance_x, 0)
            output.write(header + bitmap.ljust(512, b'\0') + bytes(4))
finally:
    gdi.SelectObject(dc, old)
    gdi.DeleteObject(font)
    gdi.DeleteDC(dc)
    gdi.RemoveFontResourceExW(font_path, 0x10, None)
print('%s: %d bytes, runtime cache only 16 glyphs' % (target, target.stat().st_size))
