#ifndef ONS_NATIVE_H
#define ONS_NATIVE_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
typedef struct { int x, y, w, h; } ons_rect;
typedef struct {
    uint8_t BitsPerPixel, BytesPerPixel;
    uint8_t Rshift, Gshift, Bshift, Ashift, Rloss, Gloss, Bloss, Aloss, alpha;
    uint32_t Rmask, Gmask, Bmask, Amask;
} ons_pixel_format;
typedef struct {
    int w, h; uint32_t *pixels;
    int pitch; uint32_t flags;
    ons_pixel_format storage, *format;
    ons_rect clip_rect;
} ons_surface;
#define ONS_SWSURFACE 0
#define ONS_SRCALPHA 1
#define ONS_ALPHA_OPAQUE 255
#define ONS_LIL_ENDIAN 1234
#define ONS_BYTEORDER ONS_LIL_ENDIAN
ons_surface *ons_surface_create(uint32_t flags, int width, int height, int depth,
    uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha);
int ons_surface_lock(ons_surface *surface);
void ons_surface_unlock(ons_surface *surface);
int ons_surface_alpha(ons_surface *surface, uint32_t flags, uint8_t alpha);
uint32_t ons_map_rgba(const ons_pixel_format *format, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ons_unpack_rgba(uint32_t pixel, const ons_pixel_format *format, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a);
ons_surface *ons_surface_new(int width, int height);
void ons_surface_free(ons_surface *surface);
uint32_t ons_over(uint32_t source, uint32_t destination);
void ons_blit(ons_surface *destination, const ons_surface *source, int x, int y);
ons_surface *ons_image_decode(const void *bytes, size_t length);
int ons_text_gbk(ons_surface *destination, const char *font_path, int size,
                 int x, int baseline, const char *text, uint32_t color);
int ons_nsa_read(const char *archive, const char *name, uint8_t **bytes, size_t *length);
/* Uncompressed entries only, for bounded streaming decoders; caller closes *file. */
int ons_nsa_stream(const char *archive, const char *name, FILE **file, uint32_t *offset, uint32_t *length);
int ons_lzss(const uint8_t *input, size_t input_size, uint8_t *output, size_t output_size);
int ons_gbk_decode(const char *text, uint32_t *codes, size_t capacity);
#endif
