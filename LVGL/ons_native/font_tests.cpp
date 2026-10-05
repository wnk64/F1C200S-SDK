#include "FontInfo.h"
#include "AnimationInfo.h"
#include "font.h"
#include "engine_types.h"
#include <cassert>
#include <cstdio>
int main(int argc, char **argv)
{
    if (argc != 2) return 1;
    FontInfo info; info.font_size_xy[0] = info.font_size_xy[1] = 30;
    assert(info.openFont(argv[1], 4, 5));
    ons_font *normal = (ons_font *)info.ttf_font[0], *outline = (ons_font *)info.ttf_font[1];
    assert(normal && outline && ons_font_height(normal) > 0 && ons_font_ascent(normal) > 0);
    int minx, maxx, miny, maxy, advance;
    assert(!ons_font_metrics(normal, 0x4e2d, &minx, &maxx, &miny, &maxy, &advance) && advance > 0 && maxx > minx && maxy > miny);
    ons_surface *glyph = ons_font_glyph(normal, 0x4e2d, 0xffffffff);
    ons_surface *stroke = ons_font_glyph(outline, 0x4e2d, 0xff000000);
    assert(glyph && stroke && stroke->w >= glyph->w && stroke->h >= glyph->h);
    AnimationInfo text; text.num_of_cells = 1; text.allocImage(64, 64, ONS_PIXELFORMAT_ARGB8888);
    ons_color white = {255, 255, 255, 255}; ons_rect clip = {0, 0, 64, 64};
    text.blendText(glyph, 4, 4, white, &clip, false);
    unsigned visible = 0;
    for (int i = 0; i < 4096; ++i) if (text.image_surface->pixels[i] >> 24) ++visible;
    assert(visible > 30);
    ons_font_style(normal, ONS_FONT_BOLD); assert(ons_font_get_style(normal) == ONS_FONT_BOLD);
    ons_surface *bold = ons_font_glyph(normal, 0x6587, 0xffffffff); assert(bold);
    ons_surface_free(bold); ons_surface_free(stroke); ons_surface_free(glyph);
    FontInfo reused; reused.font_size_xy[0] = reused.font_size_xy[1] = 24;
    assert(reused.openFont(argv[1], 1, 1) && reused.ttf_font[0] == info.ttf_font[0]);
    assert(!ons_font_open("/missing-font.ttf", 24));
    assert(ons_font_metrics(normal, 0xd800, NULL, NULL, NULL, NULL, NULL) == -1);
    std::printf("Original FontInfo -> FreeType metrics/outline/bold -> AnimationInfo PASS pixels=%u\n", visible);
    return 0;
}
