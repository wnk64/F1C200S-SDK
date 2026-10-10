#include "pixels.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    size_t bytes=1920*800;uint8_t *dst=malloc(bytes+32);uint16_t *src=malloc(640*480*2);
    assert(dst&&src);memset(dst,0xa5,bytes+32);for(unsigned i=0;i<640*480;i++)src[i]=0xf800;
    assert(retro_blit(dst,bytes,1920,src,640,480,1280,2,4.f/3)==0);
    assert(((uint32_t*)dst)[44*480]==0xff0000);
    for(size_t i=448*1920;i<bytes+32;i++)assert(dst[i]==0xa5);
    assert(retro_blit(dst,bytes,1920,src,0,480,1280,2,1)==-1);
    assert(retro_blit(dst,bytes,1920,src,640,480,1,2,1)==-1);
    uint16_t red=0x7c00;assert(retro_pixel((const uint8_t*)&red,0)==0xff0000);
    uint32_t blue=0xaa0000ff;assert(retro_pixel((const uint8_t*)&blue,1)==0xff);
    free(src);free(dst);puts("native pixel formats/aspect/stride/guarded-control-band PASS");return 0;
}
