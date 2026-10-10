#ifndef RETRO_NATIVE_PIXELS_H
#define RETRO_NATIVE_PIXELS_H
#include <stddef.h>
#include <stdint.h>
/* format0=0RGB1555,1=XRGB8888,2=RGB565, matching libretro's ABI. */
static uint32_t retro_pixel(const uint8_t *p, int format)
{
    if(format==1){uint32_t c;__builtin_memcpy(&c,p,4);return c&0xffffff;}
    uint16_t c;__builtin_memcpy(&c,p,2);
    unsigned r=(c>>(format==2?11:10))&31,g=(c>>5)&(format==2?63:31),b=c&31;
    return (r*255/31<<16)|(g*255/(format==2?63:31)<<8)|(b*255/31);
}
static int retro_blit(uint8_t *dst,size_t size,unsigned stride,const void *input,
                      unsigned w,unsigned h,size_t pitch,int format,float aspect)
{
    unsigned bpp=format==1?4:2;
    if(!dst||!input||!w||w>1024||!h||h>512||format<0||format>2||pitch<w*bpp||pitch>65536||stride<1920||stride%4||size<(size_t)stride*800)return -1;
    if(!(aspect>0.25f&&aspect<4.0f))aspect=(float)w/h;
    unsigned ow=480,oh=480/aspect;if(oh>448){oh=448;ow=448*aspect;}
    if(!ow||!oh)return -1;
    unsigned left=(480-ow)/2,top=(448-oh)/2;
    for(unsigned y=0;y<448;y++){
        uint32_t *row=(uint32_t *)(dst+y*stride);
        for(unsigned x=0;x<480;x++){
            if(x<left||x>=left+ow||y<top||y>=top+oh){row[x]=0;continue;}
            const uint8_t *p=(const uint8_t *)input+((size_t)(y-top)*h/oh)*pitch+((size_t)(x-left)*w/ow)*bpp;
            row[x]=retro_pixel(p,format);
        }
    }
    return 0;
}
#endif
