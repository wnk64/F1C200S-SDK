"""Generate a bounded LVGL 8 Chinese UI font using Windows GDI, no extra tools."""
import ctypes as c
from ctypes import wintypes as w
from pathlib import Path

TEXT = "应用无线投屏手机画面与声音触摸测试系统设置显示亮度返回上一页下一页启动失败请检查日志网络已连接未连接内存可用重试退出蓝牙选择设备暂无设备扫描已配对未配对已保存忘记此设备确认取消断开连接加密开放网络密码配对码正在扫描正在处理正在连接请稍候操作失败扫描失败不支持此认证密码需为8至63位日期时间北京年月日时分保存联网校时无法启动正在更新写入失败忙稍后重试早于编译超时"

class Point(c.Structure):
    _fields_ = [("x", w.LONG), ("y", w.LONG)]

class Metrics(c.Structure):
    _fields_ = [("width", w.UINT), ("height", w.UINT), ("origin", Point),
                ("advance_x", c.c_short), ("advance_y", c.c_short)]

class Fixed(c.Structure):
    _fields_ = [("fract", w.WORD), ("value", c.c_short)]

class Matrix(c.Structure):
    _fields_ = [("m11", Fixed), ("m12", Fixed), ("m21", Fixed), ("m22", Fixed)]

gdi = c.WinDLL("gdi32", use_last_error=True)
gdi.CreateCompatibleDC.argtypes = [w.HDC]
gdi.CreateCompatibleDC.restype = w.HDC
gdi.CreateFontW.argtypes = [c.c_int] * 5 + [w.DWORD] * 8 + [w.LPCWSTR]
gdi.CreateFontW.restype = w.HFONT
gdi.SelectObject.argtypes = [w.HDC, w.HGDIOBJ]
gdi.SelectObject.restype = w.HGDIOBJ
gdi.GetGlyphOutlineW.argtypes = [w.HDC, w.UINT, w.UINT, c.POINTER(Metrics),
                                w.DWORD, c.c_void_p, c.POINTER(Matrix)]
gdi.GetGlyphOutlineW.restype = w.DWORD
gdi.DeleteObject.argtypes = [w.HGDIOBJ]
gdi.DeleteDC.argtypes = [w.HDC]
dc = gdi.CreateCompatibleDC(None)
font = gdi.CreateFontW(-28, 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 4, 0, "Microsoft YaHei")
if not dc or not font:
    raise OSError("GDI font creation failed")
old = gdi.SelectObject(dc, font)
matrix = Matrix(Fixed(0, 1), Fixed(0, 0), Fixed(0, 0), Fixed(0, 1))
codes = sorted(set(map(ord, "，。！？：；、‘’“”（）【】《》〈〉—…·￥　\ufffd")))
bitmap, descriptors = [], ["{0}"]
try:
    for code in codes:
        metrics = Metrics()
        size = gdi.GetGlyphOutlineW(dc, code, 6, c.byref(metrics), 0, None, c.byref(matrix))
        if size == 0xffffffff:
            raise OSError("Missing glyph U+%04X" % code)
        if size == 0:
            metrics.width = metrics.height = 0
        buffer = c.create_string_buffer(size)
        gdi.GetGlyphOutlineW(dc, code, 6, c.byref(metrics), size, buffer, c.byref(matrix))
        raw = buffer.raw
        stride = (metrics.width + 3) & ~3
        pixels = [(raw[y * stride + x] * 15 + 32) // 64
                  for y in range(metrics.height) for x in range(metrics.width)]
        if len(pixels) % 2:
            pixels.append(0)
        index = len(bitmap)
        bitmap.extend((pixels[i] << 4) | pixels[i + 1] for i in range(0, len(pixels), 2))
        descriptors.append("{%d,%d,%d,%d,%d,%d}" % (index, metrics.advance_x * 16,
                           metrics.width, metrics.height, metrics.origin.x,
                           metrics.origin.y - metrics.height))
finally:
    gdi.SelectObject(dc, old)
    gdi.DeleteObject(font)
    gdi.DeleteDC(dc)

source = '#include "lvgl/lvgl.h"\nLV_FONT_DECLARE(menu_font_stream);\n/* Generated 28px/4bpp UI-only glyph subset; do not edit. */\n'
source += 'static const uint8_t bitmap[] = {\n' + ',\n'.join(
    ','.join('0x%02x' % b for b in bitmap[i:i+24]) for i in range(0, len(bitmap), 24)) + '\n};\n'
source += 'static const lv_font_fmt_txt_glyph_dsc_t glyphs[] = {\n' + ',\n'.join(descriptors) + '\n};\n'
source += 'static const uint16_t unicode[] = {' + ','.join(str(code-codes[0]) for code in codes) + '};\n'
source += '''static const lv_font_fmt_txt_cmap_t cmaps[] = {{
 .range_start = %d, .range_length = %d, .glyph_id_start = 1,
 .unicode_list = unicode, .list_length = %d, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
}};
static lv_font_fmt_txt_dsc_t descriptor = {
 .glyph_bitmap = bitmap, .glyph_dsc = glyphs, .cmaps = cmaps,
 .cmap_num = 1, .bpp = 4, .bitmap_format = 0
};
const lv_font_t reader_font_28 = {
 .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
 .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
 .line_height = 36, .base_line = 8, .dsc = &descriptor,
 .fallback = &menu_font_stream
};
''' % (codes[0], codes[-1]-codes[0]+1, len(codes))
target = Path(__file__).with_name('reader_font_28.c')
target.write_text(source, encoding='ascii', newline='\n')
print('%s: %d glyphs, %d bitmap bytes' % (target, len(codes), len(bitmap)))
