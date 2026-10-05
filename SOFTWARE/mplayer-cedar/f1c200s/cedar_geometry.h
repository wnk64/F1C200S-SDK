#ifndef CEDAR_NATIVE_GEOMETRY_H
#define CEDAR_NATIVE_GEOMETRY_H
#include "cedar_display.h"
#include <stddef.h>
struct cedar_native_view {
    int width, height, x, y, source_x, source_y, stride;
    size_t y_offset, uv_offset;
    unsigned y_tile, uv_tile;
};
static size_t cedar_tile_base(int stride, int x, int y)
{
    return (size_t)(y / 32) * stride * 32 + (size_t)(x / 32) * 1024;
}
static unsigned cedar_tile_window(int x, int y, int width)
{
    return ((unsigned)((x + width - 1) & 31) << 16) | ((unsigned)(y & 31) << 8) | (unsigned)(x & 31);
}
static int cedar_native_geometry(const struct cedar_picture_geometry *g, int screen_w, int screen_h,
                                 struct cedar_native_view *v)
{
    if (!g || !v || screen_w < 2 || screen_h < 2 || g->width < 2 || g->height < 2 ||
        g->width > 8192 || g->height > 8192 || g->stride < g->width || g->stride > 8192 ||
        (g->stride & 31)) return -1;
    int right = g->right, bottom = g->bottom;
    if (!g->left && !g->top && !right && !bottom) { right = g->width; bottom = g->height; }
    if (g->left < 0 || g->top < 0 || right <= g->left || bottom <= g->top ||
        right > g->width || bottom > g->height || (g->left & 1) || (g->top & 1)) return -1;
    int width = right - g->left, height = bottom - g->top;
    if ((width & 1) || (height & 1)) return -1;
    v->width = width < screen_w ? width : screen_w & ~1;
    v->height = height < screen_h ? height : screen_h & ~1;
    v->source_x = g->left + (((width - v->width) / 2) & ~1);
    v->source_y = g->top + (((height - v->height) / 2) & ~1);
    v->x = (screen_w - v->width) / 2; v->y = (screen_h - v->height) / 2;
    v->stride = g->stride;
    v->y_offset = cedar_tile_base(g->stride, v->source_x, v->source_y);
    v->uv_offset = cedar_tile_base(g->stride, v->source_x, v->source_y / 2);
    v->y_tile = cedar_tile_window(v->source_x, v->source_y, v->width);
    v->uv_tile = cedar_tile_window(v->source_x, v->source_y / 2, v->width);
    return 0;
}
#endif
