#include "AnimationInfo.h"
#include "engine_types.h"

ons_surface *AnimationInfo::setupImageAlpha(ons_surface *source, ons_surface *mask, bool has_alpha)
{
    if (!source || source->format->BitsPerPixel != 32 || num_of_cells < 1) return NULL;
    enum ons_transparency mode;
    switch (trans_mode) {
        case TRANS_ALPHA: case TRANS_STRING: mode = ONS_ALPHA; break;
        case TRANS_COPY: mode = ONS_COPY; break;
        case TRANS_TOPLEFT: mode = ONS_KEY_TOPLEFT; break;
        case TRANS_TOPRIGHT: mode = ONS_KEY_TOPRIGHT; break;
        case TRANS_DIRECT: mode = ONS_KEY_COLOR; break;
        case TRANS_MASK: mode = ONS_MASK; break;
        default: return NULL;
    }
    ons_sprite native = {};
    uint32_t key = (uint32_t)direct_color[0] << 16 | (uint32_t)direct_color[1] << 8 | direct_color[2];
    if (source->format->Rmask != 0xff0000 || source->format->Gmask != 0xff00 || source->format->Bmask != 0xff) return NULL;
    if (ons_sprite_open(&native, source, num_of_cells, mode, has_alpha || trans_mode == TRANS_STRING, key, mask)) return NULL;
    ons_surface *result = native.image;
    native.image = NULL; ons_sprite_close(&native);
    orig_pos.w = result->w; orig_pos.h = result->h;
    ons_surface_free(source); return result;
}
unsigned char AnimationInfo::getAlpha(int x, int y)
{
    if (!image_surface || num_of_cells < 1 || current_cell < 0 || current_cell >= num_of_cells) return 0;
    int64_t relative_x = (int64_t)x - pos.x, relative_y = (int64_t)y - pos.y;
    int width = image_surface->w / num_of_cells;
    if (relative_x < 0 || relative_x >= width || relative_y < 0 || relative_y >= image_surface->h) return 0;
    uint32_t *row = (uint32_t *)((uint8_t *)image_surface->pixels + image_surface->pitch * relative_y);
    return row[width * current_cell + relative_x] >> 24;
}
