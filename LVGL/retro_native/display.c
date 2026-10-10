#include "../menu/platform.c"
#include "pixels.h"
int retro_present(const void *p,unsigned w,unsigned h,size_t pitch,int format,float aspect)
{
    if(screen.xoffset||screen.yoffset||screen.red.offset!=16||screen.green.offset!=8||screen.blue.offset)return -1;
    return retro_blit(fb,fixed.smem_len,fixed.line_length,p,w,h,pitch,format,aspect);
}
