#ifndef ONS_NATIVE_SPRITE_H
#define ONS_NATIVE_SPRITE_H
#include "native.h"
typedef struct { int x, y, w, h; } ons_rect;
enum ons_transparency { ONS_ALPHA, ONS_COPY, ONS_KEY_TOPLEFT, ONS_KEY_TOPRIGHT, ONS_KEY_COLOR, ONS_MASK };
enum ons_blend { ONS_BLEND_OVER, ONS_BLEND_ADD, ONS_BLEND_SUBTRACT };
typedef struct { ons_surface *image; int cells, cell; } ons_sprite;
/* Source/mask remain borrowed and unmodified; destination owns its copy. */
int ons_sprite_open(ons_sprite *sprite, const ons_surface *source, int cells,
                    enum ons_transparency mode, int has_alpha, uint32_t key, const ons_surface *mask);
void ons_sprite_close(ons_sprite *sprite);
int ons_sprite_cell(ons_sprite *sprite, int cell);
uint8_t ons_sprite_hit(const ons_sprite *sprite, ons_rect placement, int x, int y, uint8_t opacity);
/* Nearest-neighbor fit of one cell; returns 1 for a dirty region, 0 for no draw, -1 invalid args. */
int ons_sprite_draw(ons_surface *destination, const ons_sprite *sprite, ons_rect placement,
                    ons_rect clip, uint8_t opacity, enum ons_blend blend, ons_rect *dirty);
#endif
