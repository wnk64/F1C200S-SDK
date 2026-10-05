#include "native.h"
#include <stdlib.h>

static int mask_details(uint32_t mask, uint8_t *shift, uint8_t *loss)
{
    *shift = 0; *loss = 8;
    if (!mask) return 0;
    while (!(mask & 1)) { ++*shift; mask >>= 1; }
    unsigned bits = 0;
    while (mask & 1) { ++bits; mask >>= 1; }
    if (mask || bits > 8) return -1;
    *loss = 8 - bits; return 0;
}
ons_surface *ons_surface_create(uint32_t flags, int w, int h, int depth,
    uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha)
{
    if (w < 1 || h < 1 || w > 2048 || h > 2048 || (size_t)w * h > 2097152 ||
        (depth != 8 && depth != 16 && depth != 32) || (red & green) || (red & blue) || (green & blue) ||
        ((red | green | blue) & alpha) || (depth < 32 && ((red | green | blue | alpha) >> depth))) return NULL;
    ons_surface *surface = calloc(1, sizeof(*surface)); if (!surface) return NULL;
    surface->format = &surface->storage;
    ons_pixel_format *format = surface->format;
    format->BitsPerPixel = depth; format->BytesPerPixel = depth / 8; format->alpha = 255;
    format->Rmask = red; format->Gmask = green; format->Bmask = blue; format->Amask = alpha;
    if (mask_details(red, &format->Rshift, &format->Rloss) || mask_details(green, &format->Gshift, &format->Gloss) ||
        mask_details(blue, &format->Bshift, &format->Bloss) || mask_details(alpha, &format->Ashift, &format->Aloss)) {
        free(surface); return NULL;
    }
    surface->pitch = (w * (depth / 8) + 3) & ~3;
    surface->pixels = calloc((size_t)surface->pitch, h);
    if (!surface->pixels) { free(surface); return NULL; }
    surface->w = w; surface->h = h; surface->flags = flags;
    surface->clip_rect = (ons_rect){0, 0, w, h}; return surface;
}
/* Native buffers are CPU memory owned by the rendering thread, not mapped hardware surfaces. */
int ons_surface_lock(ons_surface *surface) { return surface && surface->pixels ? 0 : -1; }
void ons_surface_unlock(ons_surface *surface) { (void)surface; }
int ons_surface_alpha(ons_surface *surface, uint32_t flags, uint8_t alpha)
{
    if (!surface || !surface->format) return -1;
    surface->flags = (surface->flags & ~ONS_SRCALPHA) | (flags & ONS_SRCALPHA);
    surface->format->alpha = alpha; return 0;
}
uint32_t ons_map_rgba(const ons_pixel_format *f, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!f) return 0;
    return (((uint32_t)r >> f->Rloss) << f->Rshift & f->Rmask) |
        (((uint32_t)g >> f->Gloss) << f->Gshift & f->Gmask) |
        (((uint32_t)b >> f->Bloss) << f->Bshift & f->Bmask) |
        (((uint32_t)a >> f->Aloss) << f->Ashift & f->Amask);
}
static uint8_t expand(uint32_t pixel, uint32_t mask, uint8_t shift)
{
    if (!mask) return 0;
    uint32_t maximum = mask >> shift;
    return (((pixel & mask) >> shift) * 255 + maximum / 2) / maximum;
}
void ons_unpack_rgba(uint32_t pixel, const ons_pixel_format *f, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
    if (!f) return;
    if (r) *r = expand(pixel, f->Rmask, f->Rshift);
    if (g) *g = expand(pixel, f->Gmask, f->Gshift);
    if (b) *b = expand(pixel, f->Bmask, f->Bshift);
    if (a) *a = f->Amask ? expand(pixel, f->Amask, f->Ashift) : 255;
}
