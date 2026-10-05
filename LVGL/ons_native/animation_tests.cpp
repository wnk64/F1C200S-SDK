#include "AnimationInfo.h"
#include "DirtyRect.h"
#include "engine_types.h"
#include <cassert>
#include <cstdio>
#include <climits>
int main()
{
    ons_surface *rgb565 = AnimationInfo::allocSurface(3, 2, ONS_PIXELFORMAT_RGB565);
    assert(rgb565 && rgb565->pitch == 8 && rgb565->format->BitsPerPixel == 16);
    uint32_t color = ons_map_rgba(rgb565->format, 255, 128, 0, 255);
    uint8_t r, g, b, a;
    ons_unpack_rgba(color, rgb565->format, &r, &g, &b, &a);
    assert(r == 255 && g >= 128 && g <= 132 && !b && a == 255);
    ons_surface_free(rgb565);
    assert(!ons_surface_create(0, 4, 4, 16, 0xff0000, 0xff00, 0xff, 0));
    ons_surface *destination = ons_surface_new(4, 4); assert(destination);
    for (int i = 0; i < 16; ++i) destination->pixels[i] = 0xff000000;
    {
        AnimationInfo image;
        image.num_of_cells = 2; image.trans_mode = AnimationInfo::TRANS_ALPHA;
        ons_surface *source = ons_surface_new(4, 2); assert(source);
        for (int y = 0; y < 2; ++y) {
            source->pixels[y * 4] = 0xffff0000; source->pixels[y * 4 + 1] = 0xffff0000;
            source->pixels[y * 4 + 2] = 0xff00ff00; source->pixels[y * 4 + 3] = 0xff00ff00;
        }
        image.setImage(source, ONS_PIXELFORMAT_ARGB8888);
        image.setCell(1); image.pos.x = image.pos.y = 1;
        assert(image.getAlpha(1, 1) == 255 && image.getAlpha(-1, 1) == 0 && image.getAlpha(INT_MAX, INT_MAX) == 0);
        ons_rect clip = {0, 0, 4, 4}; image.blendOnSurface(destination, 1, 1, clip, 255);
        assert(destination->pixels[5] == 0xff00ff00 && destination->pixels[6] == 0xff00ff00 && destination->pixels[0] == 0xff000000);
        image.duration_list = new int[2]; image.duration_list[0] = 20; image.duration_list[1] = 30;
        image.loop_mode = 0; image.current_cell = 0; image.remaining_time = 0;
        image.is_animatable = image.visible = true;
        assert(image.proceedAnimation() && image.current_cell == 1);
        image.stepAnimation(100); assert(image.proceedAnimation() && image.current_cell == 0);
    }
    {
        AnimationInfo mask_image;
        mask_image.num_of_cells = 2; mask_image.trans_mode = AnimationInfo::TRANS_ALPHA;
        ons_surface *source = ons_surface_new(4, 1); assert(source);
        source->pixels[0] = 0xffff0000; source->pixels[1] = 0xffffffff;
        source->pixels[2] = 0xff00ff00; source->pixels[3] = 0xff000000;
        ons_surface *result = mask_image.setupImageAlpha(source, NULL, false);
        assert(result && result->w == 2 && result->pixels[0] == 0x00ff0000 && result->pixels[1] == 0xff00ff00);
        ons_surface_free(result);
    }
    {
        AnimationInfo text;
        text.num_of_cells = 1; text.allocImage(2, 2, ONS_PIXELFORMAT_ARGB8888);
        ons_surface *glyph = ons_surface_create(0, 2, 1, 8, 0, 0, 0, 0); assert(glyph && glyph->pitch == 4);
        ((uint8_t *)glyph->pixels)[0] = 255; ((uint8_t *)glyph->pixels)[1] = 0;
        ons_color green = {0, 255, 0, 255}; ons_rect clip = {0, 0, 2, 2};
        text.blendText(glyph, 0, 0, green, &clip, false);
        assert(text.image_surface->pixels[0] == 0xff00ff00 && text.image_surface->pixels[1] == 0);
        ons_surface_free(glyph);
    }
    DirtyRect dirty; dirty.setDimension(4, 4); dirty.add((ons_rect){1, 1, 1, 1}); dirty.add((ons_rect){2, 2, 1, 1});
    assert(dirty.bounding_box.x == 1 && dirty.bounding_box.y == 1 && dirty.bounding_box.w == 2 && dirty.bounding_box.h == 2);
    ons_surface_free(destination);
    std::puts("Original AnimationInfo blend/cells/animation/mask/glyph/alpha/DirtyRect PASS (no SDL)"); return 0;
}
