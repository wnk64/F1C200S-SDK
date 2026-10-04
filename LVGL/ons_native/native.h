#ifndef ONS_NATIVE_H
#define ONS_NATIVE_H
#include <stdint.h>
#include <stddef.h>
typedef struct { int w, h; uint32_t *pixels; } ons_surface;
ons_surface *ons_surface_new(int width, int height);
void ons_surface_free(ons_surface *surface);
uint32_t ons_over(uint32_t source, uint32_t destination);
void ons_blit(ons_surface *destination, const ons_surface *source, int x, int y);
ons_surface *ons_image_decode(const void *bytes, size_t length);
int ons_text_gbk(ons_surface *destination, const char *font_path, int size,
                 int x, int baseline, const char *text, uint32_t color);
int ons_nsa_read(const char *archive, const char *name, uint8_t **bytes, size_t *length);
int ons_lzss(const uint8_t *input, size_t input_size, uint8_t *output, size_t output_size);
#endif
