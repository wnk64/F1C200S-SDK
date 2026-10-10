#ifndef SFC_BLIT_H
#define SFC_BLIT_H
#include <stddef.h>
#include <stdint.h>
static uint32_t sfc_pixel(uint16_t c)
{
    static uint32_t red[32],green[64],blue[32];
    static int ready;
    if(!ready){
        for(unsigned i=0;i<32;i++){red[i]=(i*255/31)<<16;blue[i]=i*255/31;}
        for(unsigned i=0;i<64;i++)green[i]=(i*255/63)<<8;
        ready=1;
    }
    return red[c>>11]|green[(c>>5)&63]|blue[c&31];
}
static int sfc_blit_frame(uint8_t *dst, size_t bytes, unsigned pitch, const uint16_t *src,
                    unsigned width, unsigned height, unsigned src_pitch, int clear_bars)
{
    if (!dst || !src || !width || width > 512 || !height || height > 478 ||
        src_pitch < width || pitch < 480 * 4 || pitch % 4 || bytes < (size_t)pitch * 800) return -1;
    unsigned out_w = width < 480 ? width : 480;
    unsigned out_h = height < 448 ? height : 448;
    unsigned src_left = (width-out_w)/2, src_top = (height-out_h)/2;
    unsigned left = (480-out_w)/2, top = (448-out_h)/2;
    for (unsigned y = 0; y < 448; y++) {
        uint32_t *line = (uint32_t *)(dst + y * pitch);
        if (y < top || y >= top+out_h) { if(clear_bars) for (unsigned x = 0; x < 480; x++) line[x] = 0; continue; }
        const uint16_t *row = src + (src_top+y-top)*src_pitch;
        if(clear_bars){for(unsigned x=0;x<left;x++)line[x]=0;for(unsigned x=left+out_w;x<480;x++)line[x]=0;}
        for (unsigned x = left; x < left+out_w; x++) {
            uint16_t c = row[src_left+x-left];
            line[x] = sfc_pixel(c);
        }
    }
    return 0;
}
static int sfc_blit(uint8_t *dst, size_t bytes, unsigned pitch, const uint16_t *src,
                    unsigned width, unsigned height, unsigned src_pitch)
{ return sfc_blit_frame(dst,bytes,pitch,src,width,height,src_pitch,1); }
#endif
