#include "../menu/platform.c"
#include "pixels.h"
int retro_present(const void *p,unsigned w,unsigned h,size_t pitch,int format,float aspect)
{
    if(screen.xoffset||screen.yoffset||screen.red.offset!=16||screen.green.offset!=8||screen.blue.offset)return -1;
    static unsigned previous_width,previous_height;
    int changed=w!=previous_width||h!=previous_height;
    int rc=retro_blit_frame(fb,fixed.smem_len,fixed.line_length,p,w,h,pitch,format,aspect,changed);
    if(!rc){previous_width=w;previous_height=h;}
    return rc;
}
