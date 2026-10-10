#ifndef RETRO_NATIVE_PIXELS_H
#define RETRO_NATIVE_PIXELS_H
#include <stddef.h>
#include <stdint.h>
/* format0=0RGB1555,1=XRGB8888,2=RGB565, matching libretro's ABI. */
static uint32_t retro_pixel(const uint8_t *p, int format)
{
    if(format==1){uint32_t c;__builtin_memcpy(&c,p,4);return c&0xffffff;}
    uint16_t c;__builtin_memcpy(&c,p,2);
    static uint32_t red[32],green5[32],green6[64],blue[32];
    static int ready;
    if(!ready){
        for(unsigned i=0;i<32;i++){red[i]=(i*255/31)<<16;green5[i]=(i*255/31)<<8;blue[i]=i*255/31;}
        for(unsigned i=0;i<64;i++)green6[i]=(i*255/63)<<8;
        ready=1;
    }
    return red[(c>>(format==2?11:10))&31] |
        (format==2?green6[(c>>5)&63]:green5[(c>>5)&31]) | blue[c&31];
}
static int retro_blit_frame(uint8_t *dst,size_t size,unsigned stride,const void *input,
                      unsigned w,unsigned h,size_t pitch,int format,float aspect,int clear_bars)
{
    unsigned bpp=format==1?4:2;
    if(!dst||!input||!w||w>1024||!h||h>512||format<0||format>2||pitch<w*bpp||pitch>65536||stride<1920||stride%4||size<(size_t)stride*800)return -1;
    (void)aspect;
    unsigned ow=w<480?w:480,oh=h<448?h:448;
    unsigned sx=(w-ow)/2,sy=(h-oh)/2;
    unsigned left=(480-ow)/2,top=(448-oh)/2;
    for(unsigned y=0;y<448;y++){
        uint32_t *row=(uint32_t *)(dst+y*stride);
        if(y<top||y>=top+oh){if(clear_bars)for(unsigned x=0;x<480;x++)row[x]=0;continue;}
        if(clear_bars){for(unsigned x=0;x<left;x++)row[x]=0;for(unsigned x=left+ow;x<480;x++)row[x]=0;}
        for(unsigned x=left;x<left+ow;x++){
            const uint8_t *p=(const uint8_t *)input+((size_t)(sy+y-top))*pitch+((size_t)(sx+x-left))*bpp;
            row[x]=retro_pixel(p,format);
        }
    }
    return 0;
}
static int retro_blit(uint8_t *dst,size_t size,unsigned stride,const void *input,
                      unsigned w,unsigned h,size_t pitch,int format,float aspect)
{return retro_blit_frame(dst,size,stride,input,w,h,pitch,format,aspect,1);}
#endif
