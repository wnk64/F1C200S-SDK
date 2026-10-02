#define _POSIX_C_SOURCE 200809L
#include "platform.h"
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
struct glyph {
    uint16_t width, height;
    int16_t x, y;
    uint16_t advance, reserved;
    uint8_t bitmap[512];
    uint32_t padding;
};
static struct { uint32_t code; struct glyph glyph; } cache[16];
static unsigned next;
static int font_fd = -1;
static struct glyph *lookup(uint32_t code)
{
    if (code < 0x4e00 || code >= 0xa000) return NULL;
    for (unsigned i = 0; i < 16; i++) if (cache[i].code == code) return &cache[i].glyph;
    if (font_fd < 0) {
        char path[320]; ssize_t n = readlink("/proc/self/exe", path, sizeof(path)-1);
        if (n < 0) return NULL;
        path[n] = 0; char *slash = strrchr(path, '/'); if (!slash) return NULL;
        if ((size_t)(slash - path) + 12 >= sizeof(path)) return NULL;
        strcpy(slash + 1, "cjk28.bin"); font_fd = open(path, O_RDONLY);
        if (font_fd < 0) return NULL;
        char header[16];
        if (pread(font_fd, header, 16, 0) != 16 || memcmp(header, "F1CJK28\0", 8)) { close(font_fd); font_fd = -1; return NULL; }
    }
    unsigned i = next++ % 16;
    cache[i].code = 0;
    if (pread(font_fd, &cache[i].glyph, sizeof(struct glyph), 16 + (off_t)(code - 0x4e00) * 528) != 528) return NULL;
    struct glyph *g = &cache[i].glyph;
    if (!g->width || !g->height || g->width * g->height > 1024) return NULL;
    cache[i].code = code; return g;
}
static bool descriptor(const lv_font_t *font, lv_font_glyph_dsc_t *out, uint32_t code, uint32_t following)
{
    (void)font; (void)following;
    struct glyph *g = lookup(code); if (!g) return false;
    out->adv_w = g->advance; out->box_w = g->width; out->box_h = g->height;
    out->ofs_x = g->x; out->ofs_y = g->y; out->bpp = 4; out->is_placeholder = 0;
    return true;
}
static const uint8_t *bitmap(const lv_font_t *font, uint32_t code)
{
    (void)font; struct glyph *g = lookup(code); return g ? g->bitmap : NULL;
}
const lv_font_t menu_font_stream = {
    .get_glyph_dsc = descriptor, .get_glyph_bitmap = bitmap,
    .line_height = 36, .base_line = 8, .fallback = &lv_font_montserrat_24
};
void menu_font_stream_close(void) { if (font_fd >= 0) close(font_fd); font_fd = -1; }
