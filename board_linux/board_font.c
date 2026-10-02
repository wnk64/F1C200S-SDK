#include "lvgl.h"
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

struct disk_glyph {
    uint16_t width, height;
    int16_t x, y;
    uint16_t advance, reserved;
    uint8_t bitmap[512];
    uint32_t padding;
};
struct cached_glyph { uint32_t code; uint16_t line; struct disk_glyph glyph; };
static struct cached_glyph cache[16];
static lv_font_t faces[5];
static unsigned next;
static int fd = -1;
static int scale(int value, unsigned line) {
    return value < 0 ? -((-value * (int)line + 18) / 36) : (value * (int)line + 18) / 36;
}
static struct disk_glyph *lookup(uint32_t code, unsigned line) {
    if (code < 0x4e00 || code >= 0xa000) return NULL;
    for (unsigned i = 0; i < 16; ++i) if (cache[i].code == code && cache[i].line == line) return &cache[i].glyph;
    if (fd < 0) {
        char path[320]; ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (n < 0) return NULL;
        path[n] = 0; char *slash = strrchr(path, '/');
        if (!slash || (size_t)(slash - path) + 12 >= sizeof(path)) return NULL;
        strcpy(slash + 1, "cjk28.bin"); fd = open(path, O_RDONLY);
        if (fd < 0) return NULL;
        char header[16];
        if (pread(fd, header, 16, 0) != 16 || memcmp(header, "F1CJK28\0", 8)) { close(fd); fd = -1; return NULL; }
    }
    struct disk_glyph source;
    if (pread(fd, &source, sizeof(source), 16 + (off_t)(code - 0x4e00) * 528) != 528 ||
        !source.width || !source.height || source.width * source.height > 1024) return NULL;
    struct cached_glyph *entry = &cache[next++ % 16];
    memset(entry, 0, sizeof(*entry));
    struct disk_glyph *target = &entry->glyph;
    target->width = scale(source.width, line); target->height = scale(source.height, line);
    if (!target->width) target->width = 1;
    if (!target->height) target->height = 1;
    if (target->width * target->height > 1024) return NULL;
    target->x = scale(source.x, line); target->y = scale(source.y, line);
    target->advance = scale(source.advance, line);
    for (unsigned y = 0; y < target->height; ++y) for (unsigned x = 0; x < target->width; ++x) {
        unsigned from = (y * source.height / target->height) * source.width + x * source.width / target->width;
        unsigned to = y * target->width + x;
        uint8_t pixel = (source.bitmap[from / 2] >> (from % 2 ? 0 : 4)) & 15;
        target->bitmap[to / 2] |= pixel << (to % 2 ? 0 : 4);
    }
    entry->code = code; entry->line = line;
    return target;
}
static bool descriptor(const lv_font_t *font, lv_font_glyph_dsc_t *out, uint32_t code, uint32_t next_code) {
    (void)next_code;
    struct disk_glyph *glyph = lookup(code, font->line_height);
    if (!glyph) return false;
    out->adv_w = glyph->advance; out->box_w = glyph->width; out->box_h = glyph->height;
    out->ofs_x = glyph->x; out->ofs_y = glyph->y; out->bpp = 4; out->is_placeholder = 0;
    return true;
}
static const uint8_t *bitmap(const lv_font_t *font, uint32_t code) {
    struct disk_glyph *glyph = lookup(code, font->line_height); return glyph ? glyph->bitmap : NULL;
}
const lv_font_t *board_font_for(const lv_font_t *fallback) {
    const lv_font_t *ascii[] = {&lv_font_montserrat_12, &lv_font_montserrat_14, &lv_font_montserrat_16,
                              &lv_font_montserrat_20, &lv_font_montserrat_24};
    unsigned i = 2;
    for (unsigned n = 0; n < 5; ++n) if (ascii[n] == fallback) i = n;
    if (!faces[i].get_glyph_dsc) {
        faces[i].get_glyph_dsc = descriptor; faces[i].get_glyph_bitmap = bitmap;
        faces[i].line_height = ascii[i]->line_height; faces[i].base_line = scale(8, ascii[i]->line_height);
        faces[i].fallback = ascii[i];
    }
    return &faces[i];
}
void board_font_close(void) { if (fd >= 0) close(fd); fd = -1; }
