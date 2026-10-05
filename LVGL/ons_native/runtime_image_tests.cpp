#include "engine_runtime.h"
#include <cassert>
#include <cstdio>
#include <climits>
int main(int argc, char **argv)
{
    ons_surface *surface = ons_surface_new(4, 2); assert(surface);
    ons_rect fill = {1, 0, 2, 1}; assert(!ons_rt_FillRect(surface, &fill, 0xffff0000));
    assert(surface->pixels[1] == 0xffff0000 && surface->pixels[2] == 0xffff0000 && !surface->pixels[0]);
    ons_rect from = {0, 0, 3, 1}, to = {1, 0, 0, 0};
    assert(!ons_rt_BlitSurface(surface, &from, surface, &to));
    assert(surface->pixels[0] == 0 && surface->pixels[1] == 0 && surface->pixels[2] == 0xffff0000 && surface->pixels[3] == 0xffff0000);
    ons_rect outside = {INT_MAX, INT_MIN, INT_MAX, INT_MAX}; assert(!ons_rt_FillRect(surface, &outside, 0xffffffff));
    ons_surface *small = ons_surface_create(0, 4, 2, 16, 0xf800, 0x7e0, 31, 0); assert(small);
    ons_surface *converted = ons_rt_ConvertSurface(surface, small->format, 0); assert(converted);
    uint8_t r, g, b; ons_unpack_rgba(((uint16_t *)converted->pixels)[2], converted->format, &r, &g, &b, NULL);
    assert(r == 255 && !g && !b);
    ons_surface *source = ons_surface_new(1, 1); assert(source); source->pixels[0] = 0x80ff0000;
    ons_surface_alpha(source, ONS_SRCALPHA, 255); surface->pixels[0] = 0xff0000ff;
    to = (ons_rect){0, 0, 0, 0}; assert(!ons_rt_BlitSurface(source, NULL, surface, &to)); assert(surface->pixels[0] == 0xff80007f);
    ons_surface_free(source); ons_surface_free(converted); ons_surface_free(small); ons_surface_free(surface);
    if (argc == 2) {
        ons_ttf_Font *font = ons_font_open(argv[1], 24); assert(font);
        ons_color white = {255, 255, 255, 0}, black = {0, 0, 0, 0};
        ons_surface *glyph = ons_ttf_RenderGlyph_Shaded(font, 0x4e2d, white, black); assert(glyph);
        for (int i = 0; i < glyph->w * glyph->h; ++i) assert((glyph->pixels[i] >> 24) == 255);
        ons_surface_free(glyph); ons_font_close(font);
    }
    std::puts("Native execution runtime fill/blit/overlap/convert/alpha/glyph tests PASS"); return 0;
}
