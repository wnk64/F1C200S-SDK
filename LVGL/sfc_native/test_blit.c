#include "blit.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    size_t size=480*800*4;uint8_t *dst=malloc(size+32);uint16_t *src=malloc(512*478*2);
    assert(dst&&src);memset(dst,0xa5,size+32);
    for(unsigned i=0;i<512*478;i++)src[i]=0xf800;
    assert(!sfc_blit(dst,size,1920,src,256,224,256));
    assert(((uint32_t*)dst)[480*112+112]==0xff0000);
    assert(((uint32_t*)dst)[480*112+111]==0);
    assert(((uint32_t*)dst)[480*335+367]==0xff0000);
    assert(((uint32_t*)dst)[480*336+112]==0);
    assert(((uint32_t*)dst)[0]==0);
    for(size_t i=448*1920;i<size+32;i++)assert(dst[i]==0xa5);
    assert(sfc_blit(dst,size-1,1920,src,256,224,256)==-1);
    assert(sfc_blit(dst,size,1920,src,0,224,256)==-1);
    assert(sfc_blit(dst,size,1921,src,256,224,256)==-1);
    assert(!sfc_blit(dst,size,1920,src,512,478,512));
    memset(src,0,512*478*2);src[15*512+16]=0xf800;
    assert(!sfc_blit(dst,size,1920,src,512,478,512));
    assert(((uint32_t*)dst)[0]==0xff0000 && ((uint32_t*)dst)[1]==0);
    for(unsigned i=0;i<512*478;i++)src[i]=0x07e0;
    assert(!sfc_blit(dst,size,1920,src,256,224,256));
    assert(((uint32_t*)dst)[480*112+112]==0x00ff00);
    for(unsigned i=0;i<512*478;i++)src[i]=0x001f;
    assert(!sfc_blit(dst,size,1920,src,256,224,256));
    assert(((uint32_t*)dst)[480*112+112]==0x0000ff);
    free(dst);free(src);puts("SFC RGB565/aspect/bounds/control-region PASS");return 0;
}
