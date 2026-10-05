#include "sprite.h"
#include <stdio.h>
#include <assert.h>
#include <limits.h>

int main(void)
{
    ons_surface *source = ons_surface_new(4, 1), *destination = ons_surface_new(4, 4);
    assert(source && destination);
    source->pixels[0] = 0xffff0000; source->pixels[1] = 0xffffffff;
    source->pixels[2] = 0xff00ff00; source->pixels[3] = 0xff000000;
    ons_sprite sprite = {0}; ons_rect dirty;
    assert(!ons_sprite_open(&sprite, source, 2, ONS_ALPHA, 0, 0, NULL));
    assert(sprite.image->w == 2 && sprite.image->pixels[0] == 0x00ff0000 && sprite.image->pixels[1] == 0xff00ff00);
    assert(source->pixels[0] == 0xffff0000);
    assert(!ons_sprite_cell(&sprite, 1));
    ons_rect placement = {1, 1, 2, 2}, clip = {0, 0, 4, 4};
    assert(ons_sprite_hit(&sprite, placement, 1, 1, 128) == 128);
    assert(!ons_sprite_hit(&sprite, placement, 3, 1, 255));
    assert(ons_sprite_draw(destination, &sprite, placement, clip, 255, ONS_BLEND_OVER, &dirty) == 1);
    assert(dirty.x == 1 && dirty.y == 1 && dirty.w == 2 && dirty.h == 2);
    assert(destination->pixels[5] == 0xff00ff00 && destination->pixels[10] == 0xff00ff00);
    assert(ons_sprite_draw(destination, &sprite, placement, clip, 255, ONS_BLEND_OVER, &dirty) == 0 && !dirty.w);
    assert(ons_sprite_cell(&sprite, 2) == -1);
    placement.x = INT_MAX;
    assert(ons_sprite_draw(destination, &sprite, placement, clip, 255, ONS_BLEND_OVER, &dirty) == 0);
    placement.x = INT_MIN;
    assert(ons_sprite_draw(destination, &sprite, placement, clip, 255, ONS_BLEND_OVER, &dirty) == 0);
    ons_sprite_close(&sprite);
    assert(!ons_sprite_open(&sprite, source, 1, ONS_KEY_TOPRIGHT, 1, 0, NULL));
    assert((sprite.image->pixels[3] >> 24) == 0 && (sprite.image->pixels[0] >> 24) == 255);
    ons_sprite_close(&sprite);
    assert(!ons_sprite_open(&sprite, source, 1, ONS_KEY_TOPLEFT, 1, 0, NULL));
    assert((sprite.image->pixels[0] >> 24) == 0 && (sprite.image->pixels[3] >> 24) == 255);
    ons_sprite_close(&sprite);
    assert(!ons_sprite_open(&sprite, source, 1, ONS_KEY_COLOR, 1, 0x00ff00, NULL));
    assert((sprite.image->pixels[2] >> 24) == 0);
    ons_sprite_close(&sprite);
    ons_surface *mask = ons_surface_new(2, 1); assert(mask);
    mask->pixels[0] = 0xffffffff; mask->pixels[1] = 0;
    assert(!ons_sprite_open(&sprite, source, 2, ONS_MASK, 0, 0, mask));
    assert((sprite.image->pixels[0] >> 24) == 0 && (sprite.image->pixels[1] >> 24) == 255 &&
        (sprite.image->pixels[2] >> 24) == 0 && (sprite.image->pixels[3] >> 24) == 255);
    ons_sprite_close(&sprite); ons_surface_free(mask);
    assert(ons_sprite_open(&sprite, source, 3, ONS_COPY, 0, 0, NULL) == -1);
    assert(ons_sprite_open(&sprite, source, 4, ONS_ALPHA, 0, 0, NULL) == -1);
    for (int i = 0; i < 4; ++i) source->pixels[i] = 0x80ffffff;
    assert(!ons_sprite_open(&sprite, source, 1, ONS_ALPHA, 1, 0, NULL));
    for (int i = 0; i < 16; ++i) destination->pixels[i] = 0xff808080;
    placement = (ons_rect){-1, -1, 4, 4}; clip = (ons_rect){1, 1, 1, 1};
    assert(ons_sprite_draw(destination, &sprite, placement, clip, 255, ONS_BLEND_ADD, &dirty) == 1);
    assert(destination->pixels[5] == 0xffffffff && destination->pixels[0] == 0xff808080);
    assert(dirty.x == 1 && dirty.y == 1 && dirty.w == 1 && dirty.h == 1);
    assert(ons_sprite_draw(destination, &sprite, placement, clip, 255, ONS_BLEND_SUBTRACT, &dirty) == 1);
    assert(destination->pixels[5] == 0xff7f7f7f);
    assert(ons_sprite_draw(destination, &sprite, placement, clip, 0, ONS_BLEND_OVER, &dirty) == 0);
    ons_sprite_close(&sprite);
    assert(!ons_sprite_open(&sprite, source, 1, ONS_COPY, 1, 0, NULL));
    assert((sprite.image->pixels[0] >> 24) == 255);
    ons_sprite_close(&sprite);
    ons_surface *original = source;
    assert(ons_sprite_adopt(&sprite, &source, 3) == -1 && source == original);
    assert(!ons_sprite_adopt(&sprite, &source, 2) && !source && sprite.image == original);
    ons_sprite_close(&sprite); ons_surface_free(destination);
    puts("Native sprite cells/keys/mask/alpha/hit/scale/clip/dirty/blend tests PASS (no SDL)");
    return 0;
}
