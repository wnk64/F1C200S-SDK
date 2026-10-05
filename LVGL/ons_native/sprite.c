#include "sprite.h"
#include <string.h>
#include <stdint.h>

int ons_sprite_adopt(ons_sprite *sprite, ons_surface **source, int cells)
{
    if (!sprite || !source || !*source || !(*source)->pixels || cells < 1 ||
        cells > (*source)->w || (*source)->w % cells || (*source)->h < 1) return -1;
    sprite->image = *source; sprite->cells = cells; sprite->cell = 0;
    *source = NULL; return 0;
}

int ons_sprite_open(ons_sprite *sprite, const ons_surface *source, int cells,
                    enum ons_transparency mode, int has_alpha, uint32_t key, const ons_surface *mask)
{
    if (!sprite || !source || !source->pixels || source->w < 1 || source->h < 1 ||
        cells < 1 || cells > source->w || source->w % cells || mode < ONS_ALPHA || mode > ONS_MASK ||
        (mode == ONS_MASK && (!mask || !mask->pixels || mask->w < 1 || mask->h < 1))) return -1;
    int cell_width = source->w / cells;
    int split = mode == ONS_ALPHA && !has_alpha;
    if (split && (cell_width < 2 || cell_width % 2)) return -1;
    int output_cell = split ? cell_width / 2 : cell_width;
    ons_surface *image = ons_surface_new(output_cell * cells, source->h);
    if (!image) return -1;
    if (mode == ONS_KEY_TOPLEFT) key = source->pixels[0];
    if (mode == ONS_KEY_TOPRIGHT) key = source->pixels[source->w - 1];
    key &= 0xffffff;
    for (int y = 0; y < image->h; ++y)
        for (int cell = 0; cell < cells; ++cell)
            for (int x = 0; x < output_cell; ++x) {
                uint32_t pixel = source->pixels[y * source->w + cell * cell_width + x];
                unsigned alpha = pixel >> 24;
                if (split) alpha = 255 - (source->pixels[y * source->w + cell * cell_width + output_cell + x] & 255);
                else if (mode == ONS_COPY) alpha = 255;
                else if (mode == ONS_MASK) alpha = 255 - (mask->pixels[(y % mask->h) * mask->w + x % mask->w] & 255);
                else if (mode == ONS_KEY_TOPLEFT || mode == ONS_KEY_TOPRIGHT || mode == ONS_KEY_COLOR)
                    alpha = (pixel & 0xffffff) == key ? 0 : 255;
                image->pixels[y * image->w + cell * output_cell + x] = (pixel & 0xffffff) | alpha << 24;
            }
    sprite->image = image; sprite->cells = cells; sprite->cell = 0; return 0;
}
void ons_sprite_close(ons_sprite *sprite)
{
    if (!sprite) return;
    ons_surface_free(sprite->image); memset(sprite, 0, sizeof(*sprite));
}
int ons_sprite_cell(ons_sprite *sprite, int cell)
{
    if (!sprite || !sprite->image || cell < 0 || cell >= sprite->cells) return -1;
    sprite->cell = cell; return 0;
}
static int valid(const ons_sprite *sprite, ons_rect placement)
{
    return sprite && sprite->image && sprite->image->pixels && sprite->cells > 0 &&
        sprite->image->w % sprite->cells == 0 && sprite->cell >= 0 && sprite->cell < sprite->cells &&
        placement.w > 0 && placement.h > 0;
}
static uint32_t sample(const ons_sprite *sprite, ons_rect placement, int x, int y, uint8_t opacity)
{
    int cw = sprite->image->w / sprite->cells;
    int sx = (int)(((int64_t)x - placement.x) * cw / placement.w);
    int sy = (int)(((int64_t)y - placement.y) * sprite->image->h / placement.h);
    uint32_t pixel = sprite->image->pixels[sy * sprite->image->w + sprite->cell * cw + sx];
    unsigned alpha = ((pixel >> 24) * opacity + 127) / 255;
    return (pixel & 0xffffff) | alpha << 24;
}
uint8_t ons_sprite_hit(const ons_sprite *sprite, ons_rect placement, int x, int y, uint8_t opacity)
{
    if (!valid(sprite, placement) || x < placement.x || y < placement.y ||
        (int64_t)x >= (int64_t)placement.x + placement.w || (int64_t)y >= (int64_t)placement.y + placement.h) return 0;
    return sample(sprite, placement, x, y, opacity) >> 24;
}
static int64_t maximum(int64_t a, int64_t b) { return a > b ? a : b; }
static int64_t minimum(int64_t a, int64_t b) { return a < b ? a : b; }
static uint32_t composite(uint32_t source, uint32_t destination, enum ons_blend blend)
{
    if (blend == ONS_BLEND_OVER) return ons_over(source, destination);
    unsigned alpha = source >> 24;
    if (!alpha) return destination;
    uint32_t result = destination & 0xff000000u;
    for (unsigned shift = 0; shift < 24; shift += 8) {
        int contribution = (((source >> shift) & 255) * alpha + 127) / 255;
        int value = (destination >> shift) & 255;
        value += blend == ONS_BLEND_ADD ? contribution : -contribution;
        if (value > 255) value = 255;
        if (value < 0) value = 0;
        result |= (uint32_t)value << shift;
    }
    return result;
}
int ons_sprite_draw(ons_surface *destination, const ons_sprite *sprite, ons_rect placement,
                    ons_rect clip, uint8_t opacity, enum ons_blend blend, ons_rect *dirty)
{
    if (dirty) memset(dirty, 0, sizeof(*dirty));
    if (!destination || !destination->pixels || !valid(sprite, placement) ||
        blend < ONS_BLEND_OVER || blend > ONS_BLEND_SUBTRACT || clip.w < 0 || clip.h < 0) return -1;
    if (!opacity || !clip.w || !clip.h) return 0;
    int x1 = (int)maximum(0, maximum(placement.x, clip.x));
    int y1 = (int)maximum(0, maximum(placement.y, clip.y));
    int64_t x2 = minimum(destination->w, minimum((int64_t)placement.x + placement.w, (int64_t)clip.x + clip.w));
    int64_t y2 = minimum(destination->h, minimum((int64_t)placement.y + placement.h, (int64_t)clip.y + clip.h));
    if (x2 <= x1 || y2 <= y1) return 0;
    int touched = 0, min_x = (int)x2, min_y = (int)y2, max_x = x1, max_y = y1;
    for (int y = y1; y < y2; ++y)
        for (int x = x1; x < x2; ++x) {
            uint32_t *pixel = destination->pixels + y * destination->w + x;
            uint32_t next = composite(sample(sprite, placement, x, y, opacity), *pixel, blend);
            if (next == *pixel) continue;
            *pixel = next; touched = 1;
            if (x < min_x) min_x = x;
            if (y < min_y) min_y = y;
            if (x > max_x) max_x = x;
            if (y > max_y) max_y = y;
        }
    if (dirty && touched) { dirty->x = min_x; dirty->y = min_y; dirty->w = max_x - min_x + 1; dirty->h = max_y - min_y + 1; }
    return touched;
}
