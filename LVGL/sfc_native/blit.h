#ifndef SFC_BLIT_H
#define SFC_BLIT_H
#include <stddef.h>
#include <stdint.h>
static int sfc_blit(uint8_t *dst, size_t bytes, unsigned pitch, const uint16_t *src,
                    unsigned width, unsigned height, unsigned src_pitch)
{
    if (!dst || !src || !width || width > 512 || !height || height > 478 ||
        src_pitch < width || pitch < 480 * 4 || pitch % 4 || bytes < (size_t)pitch * 800) return -1;
    unsigned out_w = width < 480 ? width : 480;
    unsigned out_h = height < 448 ? height : 448;
    unsigned src_left = (width-out_w)/2, src_top = (height-out_h)/2;
    unsigned left = (480-out_w)/2, top = (448-out_h)/2;
    for (unsigned y = 0; y < 448; y++) {
        uint32_t *line = (uint32_t *)(dst + y * pitch);
        if (y < top || y >= top+out_h) { for (unsigned x = 0; x < 480; x++) line[x] = 0; continue; }
        const uint16_t *row = src + (src_top+y-top)*src_pitch;
        for (unsigned x = 0; x < 480; x++) {
            if (x < left || x >= left+out_w) { line[x] = 0; continue; }
            uint16_t c = row[src_left+x-left];
            unsigned r = c >> 11, g = (c >> 5) & 63, b = c & 31;
            line[x] = ((r*255/31)<<16) | ((g*255/63)<<8) | (b*255/31);
        }
    }
    return 0;
}
#endif
