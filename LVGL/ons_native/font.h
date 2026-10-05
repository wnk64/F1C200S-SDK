#ifndef ONS_NATIVE_FONT_H
#define ONS_NATIVE_FONT_H
#include "native.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct ons_font ons_font;
enum { ONS_FONT_NORMAL = 0, ONS_FONT_BOLD = 1 };
ons_font *ons_font_open(const char *path, int size);
void ons_font_close(ons_font *font);
int ons_font_height(const ons_font *font);
int ons_font_ascent(const ons_font *font);
void ons_font_style(ons_font *font, int style);
int ons_font_get_style(const ons_font *font);
void ons_font_outline(ons_font *font, int pixels);
int ons_font_metrics(ons_font *font, uint32_t code, int *min_x, int *max_x, int *min_y, int *max_y, int *advance);
ons_surface *ons_font_glyph(ons_font *font, uint32_t code, uint32_t color);
#ifdef __cplusplus
}
#endif
#endif
