#include "font.h"
#include <stdlib.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_STROKER_H
#include FT_SYNTHESIS_H

struct ons_font { FT_Library library; FT_Face face; int style, outline; };
ons_font *ons_font_open(const char *path, int size)
{
    if (!path || size < 1 || size > 128) return NULL;
    ons_font *font = calloc(1, sizeof(*font)); if (!font) return NULL;
    if (FT_Init_FreeType(&font->library) || FT_New_Face(font->library, path, 0, &font->face) ||
        FT_Set_Pixel_Sizes(font->face, 0, size)) { ons_font_close(font); return NULL; }
    return font;
}
void ons_font_close(ons_font *font)
{
    if (!font) return;
    if (font->face) FT_Done_Face(font->face);
    if (font->library) FT_Done_FreeType(font->library);
    free(font);
}
int ons_font_height(const ons_font *font) { return font ? (font->face->size->metrics.height + 63) >> 6 : 0; }
int ons_font_ascent(const ons_font *font) { return font ? (font->face->size->metrics.ascender + 63) >> 6 : 0; }
void ons_font_style(ons_font *font, int style) { if (font) font->style = style & ONS_FONT_BOLD; }
int ons_font_get_style(const ons_font *font) { return font ? font->style : 0; }
void ons_font_outline(ons_font *font, int pixels) { if (font) font->outline = pixels < 0 ? 0 : pixels > 8 ? 8 : pixels; }
static int load(ons_font *font, uint32_t code)
{
    if (!font || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff) || FT_Load_Char(font->face, code, FT_LOAD_DEFAULT)) return -1;
    if (font->style & ONS_FONT_BOLD) FT_GlyphSlot_Embolden(font->face->glyph);
    return 0;
}
int ons_font_metrics(ons_font *font, uint32_t code, int *min_x, int *max_x, int *min_y, int *max_y, int *advance)
{
    if (load(font, code)) return -1;
    FT_Glyph_Metrics *metrics = &font->face->glyph->metrics;
    if (min_x) *min_x = (metrics->horiBearingX >> 6) - font->outline;
    if (max_x) *max_x = ((metrics->horiBearingX + metrics->width + 63) >> 6) + font->outline;
    if (min_y) *min_y = ((metrics->horiBearingY - metrics->height) >> 6) - font->outline;
    if (max_y) *max_y = ((metrics->horiBearingY + 63) >> 6) + font->outline;
    if (advance) *advance = (font->face->glyph->advance.x + 63) >> 6;
    return 0;
}
ons_surface *ons_font_glyph(ons_font *font, uint32_t code, uint32_t color)
{
    if (load(font, code)) return NULL;
    FT_Glyph glyph = NULL; FT_Stroker stroker = NULL; ons_surface *surface = NULL;
    if (FT_Get_Glyph(font->face->glyph, &glyph)) goto done;
    if (font->outline) {
        if (FT_Stroker_New(font->library, &stroker)) goto done;
        FT_Stroker_Set(stroker, font->outline * 64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
        if (FT_Glyph_StrokeBorder(&glyph, stroker, 0, 1)) goto done;
    }
    if (FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, NULL, 1)) goto done;
    FT_Bitmap *bitmap = &((FT_BitmapGlyph)glyph)->bitmap;
    if (bitmap->pixel_mode != FT_PIXEL_MODE_GRAY && bitmap->rows) goto done;
    surface = ons_surface_new(bitmap->width ? bitmap->width : 1, bitmap->rows ? bitmap->rows : 1);
    if (!surface) goto done;
    for (unsigned y = 0; y < bitmap->rows; ++y) {
        const uint8_t *row = bitmap->buffer + (bitmap->pitch >= 0 ? (int)y * bitmap->pitch :
            (int)(bitmap->rows - 1 - y) * -bitmap->pitch);
        for (unsigned x = 0; x < bitmap->width; ++x) {
            unsigned alpha = (row[x] * (color >> 24) + 127) / 255;
            surface->pixels[y * surface->w + x] = (color & 0xffffff) | alpha << 24;
        }
    }
done:
    if (stroker) FT_Stroker_Done(stroker);
    if (glyph) FT_Done_Glyph(glyph);
    return surface;
}
