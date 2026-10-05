#include "engine_runtime.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static uint32_t read_pixel(const ons_surface *surface, int x, int y)
{
    const uint8_t *row = (const uint8_t *)surface->pixels + y * surface->pitch;
    uint32_t pixel = 0; std::memcpy(&pixel, row + x * surface->format->BytesPerPixel, surface->format->BytesPerPixel);
    return pixel;
}
static void write_pixel(ons_surface *surface, int x, int y, uint32_t pixel)
{
    uint8_t *row = (uint8_t *)surface->pixels + y * surface->pitch;
    std::memcpy(row + x * surface->format->BytesPerPixel, &pixel, surface->format->BytesPerPixel);
}
static uint32_t argb(const ons_surface *surface, uint32_t pixel)
{
    uint8_t r, g, b, a; ons_unpack_rgba(pixel, surface->format, &r, &g, &b, &a);
    return (uint32_t)a << 24 | (uint32_t)r << 16 | (uint32_t)g << 8 | b;
}
static uint32_t packed(ons_surface *surface, uint32_t pixel)
{
    return ons_map_rgba(surface->format, pixel >> 16, pixel >> 8, pixel, pixel >> 24);
}
int ons_rt_FillRect(ons_surface *surface, ons_rect *rect, uint32_t pixel)
{
    if (!surface || !surface->pixels) return -1;
    ons_rect target = rect ? *rect : ons_rect{0, 0, surface->w, surface->h};
    if (target.w < 0 || target.h < 0) return -1;
    int x1 = std::max(0, std::max(target.x, surface->clip_rect.x));
    int y1 = std::max(0, std::max(target.y, surface->clip_rect.y));
    int64_t x2 = std::min<int64_t>(surface->w, std::min<int64_t>((int64_t)target.x + target.w,
        (int64_t)surface->clip_rect.x + surface->clip_rect.w));
    int64_t y2 = std::min<int64_t>(surface->h, std::min<int64_t>((int64_t)target.y + target.h,
        (int64_t)surface->clip_rect.y + surface->clip_rect.h));
    for (int y = y1; y < y2; ++y) for (int x = x1; x < x2; ++x) write_pixel(surface, x, y, pixel);
    return 0;
}
int ons_rt_BlitSurface(ons_surface *source, ons_rect *from, ons_surface *destination, ons_rect *to)
{
    if (!source || !destination || !source->pixels || !destination->pixels) return -1;
    ons_rect src = from ? *from : ons_rect{0, 0, source->w, source->h};
    int64_t dx = to ? to->x : 0, dy = to ? to->y : 0;
    if (src.w < 0 || src.h < 0) return -1;
    int64_t first_x = std::max<int64_t>(0, std::max<int64_t>(-(int64_t)src.x, std::max<int64_t>(-dx, (int64_t)destination->clip_rect.x - dx)));
    int64_t first_y = std::max<int64_t>(0, std::max<int64_t>(-(int64_t)src.y, std::max<int64_t>(-dy, (int64_t)destination->clip_rect.y - dy)));
    int64_t end_x = std::min<int64_t>(src.w, std::min<int64_t>((int64_t)source->w - src.x,
        std::min<int64_t>(destination->w - dx, (int64_t)destination->clip_rect.x + destination->clip_rect.w - dx)));
    int64_t end_y = std::min<int64_t>(src.h, std::min<int64_t>((int64_t)source->h - src.y,
        std::min<int64_t>(destination->h - dy, (int64_t)destination->clip_rect.y + destination->clip_rect.h - dy)));
    if (to) { to->w = end_x > first_x ? end_x - first_x : 0; to->h = end_y > first_y ? end_y - first_y : 0; }
    if (end_x <= first_x || end_y <= first_y) return 0;
    bool identical = source->format->BitsPerPixel == destination->format->BitsPerPixel &&
        source->format->Rmask == destination->format->Rmask && source->format->Gmask == destination->format->Gmask &&
        source->format->Bmask == destination->format->Bmask && source->format->Amask == destination->format->Amask;
    if (identical && !(source->flags & ONS_SRCALPHA)) {
        size_t bytes = (end_x - first_x) * source->format->BytesPerPixel;
        bool reverse = source == destination && dy > src.y;
        for (int64_t row = 0; row < end_y - first_y; ++row) {
            int64_t y = reverse ? end_y - 1 - row : first_y + row;
            const uint8_t *input = (const uint8_t *)source->pixels + (src.y + y) * source->pitch +
                (src.x + first_x) * source->format->BytesPerPixel;
            uint8_t *output = (uint8_t *)destination->pixels + (dy + y) * destination->pitch +
                (dx + first_x) * destination->format->BytesPerPixel;
            std::memmove(output, input, bytes);
        }
        return 0;
    }
    std::vector<uint32_t> overlap;
    if (source == destination) {
        try { overlap.resize((end_x - first_x) * (end_y - first_y)); }
        catch (...) { return -1; }
        for (int64_t y = first_y; y < end_y; ++y) for (int64_t x = first_x; x < end_x; ++x)
            overlap[(y - first_y) * (end_x - first_x) + x - first_x] = read_pixel(source, src.x + x, src.y + y);
    }
    for (int64_t y = first_y; y < end_y; ++y) for (int64_t x = first_x; x < end_x; ++x) {
        uint32_t pixel = overlap.empty() ? read_pixel(source, src.x + x, src.y + y) :
            overlap[(y - first_y) * (end_x - first_x) + x - first_x];
        if (source->flags & ONS_SRCALPHA) {
            uint32_t rgba = argb(source, pixel);
            unsigned alpha = ((rgba >> 24) * source->format->alpha + 127) / 255;
            rgba = (rgba & 0xffffff) | alpha << 24;
            pixel = packed(destination, ons_over(rgba, argb(destination, read_pixel(destination, dx + x, dy + y))));
        } else if (!identical) pixel = packed(destination, argb(source, pixel));
        write_pixel(destination, dx + x, dy + y, pixel);
    }
    return 0;
}
ons_surface *ons_rt_ConvertSurface(ons_surface *source, ons_pixel_format *format, uint32_t flags)
{
    if (!source || !format) return NULL;
    ons_surface *destination = ons_surface_create(flags, source->w, source->h, format->BitsPerPixel,
        format->Rmask, format->Gmask, format->Bmask, format->Amask);
    if (!destination) return NULL;
    for (int y = 0; y < source->h; ++y) for (int x = 0; x < source->w; ++x)
        write_pixel(destination, x, y, packed(destination, argb(source, read_pixel(source, x, y))));
    destination->format->alpha = source->format->alpha; return destination;
}
static void little32(uint8_t *output, uint32_t number)
{
    for (unsigned i = 0; i < 4; ++i) output[i] = number >> (i * 8);
}
int ons_rt_SaveBMP(ons_surface *surface, const char *path)
{
    if (!surface || !surface->pixels || !path) return -1;
    std::FILE *file = std::fopen(path, "wb"); if (!file) return -1;
    uint8_t header[54] = {}; unsigned stride = (surface->w * 3 + 3) & ~3;
    header[0] = 'B'; header[1] = 'M'; header[10] = 54; header[14] = 40;
    header[26] = 1; header[28] = 24;
    little32(header + 2, 54 + stride * surface->h);
    little32(header + 18, surface->w); little32(header + 22, surface->h);
    little32(header + 34, stride * surface->h);
    int result = std::fwrite(header, 1, sizeof(header), file) == sizeof(header) ? 0 : -1;
    std::vector<uint8_t> row;
    try { row.resize(stride); } catch (...) { std::fclose(file); return -1; }
    for (int y = surface->h - 1; y >= 0 && !result; --y) {
        for (int x = 0; x < surface->w; ++x) {
            uint32_t pixel = argb(surface, read_pixel(surface, x, y));
            row[x * 3] = pixel; row[x * 3 + 1] = pixel >> 8; row[x * 3 + 2] = pixel >> 16;
        }
        if (std::fwrite(&row[0], 1, row.size(), file) != row.size()) result = -1;
    }
    if (std::fclose(file)) result = -1;
    return result;
}
uint32_t ons_rt_MapRGB(const ons_pixel_format *format, uint8_t r, uint8_t g, uint8_t b) { return ons_map_rgba(format, r, g, b, 255); }
uint32_t ons_rt_MapRGBA(const ons_pixel_format *format, uint8_t r, uint8_t g, uint8_t b, uint8_t a) { return ons_map_rgba(format, r, g, b, a); }
ons_rt_RWops *ons_rt_RWFromMem(void *memory, int size)
{
    if (!memory || size < 1) return NULL;
    ons_rt_RWops *stream = (ons_rt_RWops *)std::calloc(1, sizeof(*stream));
    if (stream) { stream->memory = (uint8_t *)memory; stream->length = size; }
    return stream;
}
int ons_rt_RWclose(ons_rt_RWops *stream) { std::free(stream); return 0; }
int ons_img_isPNG(ons_rt_RWops *stream)
{
    static const uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    return stream && stream->length - stream->position >= 8 && !std::memcmp(stream->memory + stream->position, signature, 8);
}
ons_surface *ons_img_Load_RW(ons_rt_RWops *stream, int free_stream)
{
    if (!stream) return NULL;
    ons_surface *surface = ons_image_decode(stream->memory + stream->position, stream->length - stream->position);
    if (surface) stream->position = stream->length;
    if (free_stream) ons_rt_RWclose(stream);
    return surface;
}
ons_surface *ons_img_LoadJPG_RW(ons_rt_RWops *stream)
{
    if (!stream || stream->length - stream->position < 2 || stream->memory[stream->position] != 255 ||
        stream->memory[stream->position + 1] != 216) return NULL;
    return ons_img_Load_RW(stream, 0);
}
const char *ons_img_GetError(void) { return "Native image decoder rejected the input"; }
int ons_ttf_Init(void) { return 0; }
const char *ons_ttf_GetError(void) { return "Native FreeType glyph operation failed"; }
ons_surface *ons_ttf_RenderGlyph_Blended(ons_ttf_Font *font, uint16_t glyph, ons_color color)
{
    return ons_font_glyph(font, glyph, 0xff000000u | (uint32_t)color.r << 16 | (uint32_t)color.g << 8 | color.b);
}
ons_surface *ons_ttf_RenderGlyph_Shaded(ons_ttf_Font *font, uint16_t glyph, ons_color color, ons_color background)
{
    ons_surface *surface = ons_ttf_RenderGlyph_Blended(font, glyph, color); if (!surface) return NULL;
    uint32_t bg = 0xff000000u | (uint32_t)background.r << 16 | (uint32_t)background.g << 8 | background.b;
    for (int y = 0; y < surface->h; ++y) for (int x = 0; x < surface->w; ++x)
        surface->pixels[y * surface->w + x] = ons_over(surface->pixels[y * surface->w + x], bg);
    return surface;
}
