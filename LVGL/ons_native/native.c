#include "native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <iconv.h>
#include <png.h>
#include <jpeglib.h>
#include <ft2build.h>
#include FT_FREETYPE_H

ons_surface *ons_surface_new(int w, int h)
{
    if (w < 1 || h < 1 || w > 2048 || h > 2048 || (size_t)w * h > 2097152) return NULL;
    ons_surface *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->pixels = calloc((size_t)w * h, sizeof(*s->pixels));
    if (!s->pixels) { free(s); return NULL; }
    s->w = w; s->h = h; return s;
}
void ons_surface_free(ons_surface *s) { if (s) { free(s->pixels); free(s); } }
uint32_t ons_over(uint32_t s, uint32_t d)
{
    unsigned a = s >> 24, da = d >> 24;
    if (!a) return d;
    if (a == 255) return s;
    unsigned oa = a * 255 + da * (255 - a);
    uint32_t result = ((oa + 127) / 255) << 24;
    for (unsigned shift = 0; shift < 24; shift += 8) {
        unsigned sc = (s >> shift) & 255, dc = (d >> shift) & 255;
        unsigned channel = (sc * a * 255 + dc * da * (255 - a) + oa / 2) / oa;
        result |= channel << shift;
    }
    return result;
}
void ons_blit(ons_surface *d, const ons_surface *s, int x, int y)
{
    if (!d || !s || x <= -s->w || y <= -s->h || x >= d->w || y >= d->h) return;
    int first_x = x < 0 ? -x : 0, first_y = y < 0 ? -y : 0;
    int end_x = s->w < d->w - x ? s->w : d->w - x;
    int end_y = s->h < d->h - y ? s->h : d->h - y;
    for (int sy = first_y; sy < end_y; ++sy)
        for (int sx = first_x; sx < end_x; ++sx) {
            uint32_t *pixel = d->pixels + (sy + y) * d->w + sx + x;
            *pixel = ons_over(s->pixels[sy * s->w + sx], *pixel);
        }
}
struct jpeg_error { struct jpeg_error_mgr base; jmp_buf jump; };
struct jpeg_decoder {
    struct jpeg_decompress_struct jpeg;
    struct jpeg_error error;
    ons_surface *surface;
    uint8_t *row;
    int created;
};
static void jpeg_fail(j_common_ptr jpeg)
{
    struct jpeg_error *error = (struct jpeg_error *)jpeg->err;
    longjmp(error->jump, 1);
}
static ons_surface *read_jpeg(const uint8_t *bytes, size_t size)
{
    struct jpeg_decoder *d = calloc(1, sizeof(*d));
    if (!d) return NULL;
    d->jpeg.err = jpeg_std_error(&d->error.base); d->error.base.error_exit = jpeg_fail;
    if (setjmp(d->error.jump)) {
        if (d->created) jpeg_destroy_decompress(&d->jpeg);
        ons_surface_free(d->surface); free(d->row); free(d); return NULL;
    }
    jpeg_create_decompress(&d->jpeg); d->created = 1;
    jpeg_mem_src(&d->jpeg, bytes, size); jpeg_read_header(&d->jpeg, TRUE);
    d->surface = ons_surface_new(d->jpeg.image_width, d->jpeg.image_height);
    if (!d->surface) jpeg_fail((j_common_ptr)&d->jpeg);
    d->jpeg.out_color_space = JCS_RGB;
    jpeg_start_decompress(&d->jpeg);
    d->row = malloc((size_t)d->jpeg.output_width * 3);
    if (!d->row) jpeg_fail((j_common_ptr)&d->jpeg);
    while (d->jpeg.output_scanline < d->jpeg.output_height) {
        unsigned y = d->jpeg.output_scanline;
        JSAMPROW row = d->row;
        if (jpeg_read_scanlines(&d->jpeg, &row, 1) != 1) jpeg_fail((j_common_ptr)&d->jpeg);
        for (unsigned x = 0; x < d->jpeg.output_width; ++x)
            d->surface->pixels[y * d->surface->w + x] = 0xff000000u |
                (uint32_t)row[x * 3] << 16 | (uint32_t)row[x * 3 + 1] << 8 | row[x * 3 + 2];
    }
    jpeg_finish_decompress(&d->jpeg); jpeg_destroy_decompress(&d->jpeg);
    ons_surface *result = d->surface;
    free(d->row); free(d); return result;
}
static ons_surface *read_png(const uint8_t *bytes, size_t size)
{
    png_image png; memset(&png, 0, sizeof(png)); png.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&png, bytes, size)) { png_image_free(&png); return NULL; }
    ons_surface *result = ons_surface_new(png.width, png.height);
    if (!result) { png_image_free(&png); return NULL; }
    png.format = PNG_FORMAT_BGRA;
    if (!png_image_finish_read(&png, NULL, result->pixels, 0, NULL)) {
        png_image_free(&png); ons_surface_free(result); return NULL;
    }
    png_image_free(&png); return result;
}
static uint16_t le16(const uint8_t *p) { return p[0] | (uint16_t)p[1] << 8; }
static uint32_t le32(const uint8_t *p) { return le16(p) | (uint32_t)le16(p + 2) << 16; }
static ons_surface *read_bmp(const uint8_t *bytes, size_t size)
{
    if (size < 54 || le32(bytes + 14) < 40 || le16(bytes + 26) != 1 || le32(bytes + 30)) return NULL;
    uint32_t offset = le32(bytes + 10);
    int32_t w = (int32_t)le32(bytes + 18), raw_h = (int32_t)le32(bytes + 22);
    unsigned depth = le16(bytes + 28);
    if (w < 1 || w > 2048 || !raw_h || raw_h < -2048 || raw_h > 2048 || (depth != 24 && depth != 32)) return NULL;
    int h = raw_h < 0 ? -raw_h : raw_h;
    size_t stride = ((size_t)w * (depth / 8) + 3) & ~(size_t)3;
    if (offset < 54 || offset > size || stride * h > size - offset) return NULL;
    ons_surface *result = ons_surface_new(w, h);
    if (!result) return NULL;
    for (int y = 0; y < h; ++y) {
        const uint8_t *row = bytes + offset + (raw_h < 0 ? y : h - 1 - y) * stride;
        for (int x = 0; x < w; ++x) {
            const uint8_t *p = row + x * (depth / 8);
            result->pixels[y * w + x] = 0xff000000u | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0];
        }
    }
    return result;
}
ons_surface *ons_image_decode(const void *input, size_t length)
{
    const uint8_t *bytes = input;
    if (!bytes || length < 2 || length > 8388608) return NULL;
    if (bytes[0] == 0xff && bytes[1] == 0xd8) return read_jpeg(bytes, length);
    if (length >= 8 && !png_sig_cmp(bytes, 0, 8)) return read_png(bytes, length);
    if (bytes[0] == 'B' && bytes[1] == 'M') return read_bmp(bytes, length);
    return NULL;
}
int ons_text_gbk(ons_surface *d, const char *path, int size, int x, int baseline, const char *text, uint32_t color)
{
    if (!d || !text || size < 1 || size > 128 || strlen(text) > 2048) return -1;
    uint32_t codes[2048];
    iconv_t cd = iconv_open("UTF-32LE", "GBK");
    if (cd == (iconv_t)-1) return -1;
    char *input = (char *)text, *output = (char *)codes;
    size_t left = strlen(text), room = sizeof(codes);
    size_t converted = iconv(cd, &input, &left, &output, &room); iconv_close(cd);
    if (converted == (size_t)-1) return -1;
    FT_Library library; FT_Face face;
    if (FT_Init_FreeType(&library)) return -1;
    if (FT_New_Face(library, path, 0, &face)) { FT_Done_FreeType(library); return -1; }
    if (FT_Set_Pixel_Sizes(face, 0, size)) { FT_Done_Face(face); FT_Done_FreeType(library); return -1; }
    int count = 0;
    for (size_t i = 0; i < (sizeof(codes) - room) / 4; ++i) {
        if (FT_Load_Char(face, codes[i], FT_LOAD_RENDER)) continue;
        FT_GlyphSlot glyph = face->glyph; FT_Bitmap *bitmap = &glyph->bitmap;
        if (bitmap->pixel_mode != FT_PIXEL_MODE_GRAY) continue;
        for (unsigned gy = 0; gy < bitmap->rows; ++gy) {
            const uint8_t *row = bitmap->buffer + (bitmap->pitch >= 0 ? (int)gy * bitmap->pitch :
                (int)(bitmap->rows - 1 - gy) * -bitmap->pitch);
            int dy = baseline - glyph->bitmap_top + gy;
            if (dy < 0 || dy >= d->h) continue;
            for (unsigned gx = 0; gx < bitmap->width; ++gx) {
                int dx = x + glyph->bitmap_left + gx;
                if (dx < 0 || dx >= d->w || !row[gx]) continue;
                uint32_t *pixel = d->pixels + dy * d->w + dx;
                unsigned a = (row[gx] * (color >> 24) + 127) / 255;
                *pixel = ons_over((color & 0xffffffu) | a << 24, *pixel);
            }
        }
        x += glyph->advance.x >> 6; ++count;
    }
    FT_Done_Face(face); FT_Done_FreeType(library); return count;
}
