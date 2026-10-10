#ifndef SFC_FRAME_POLICY_H
#define SFC_FRAME_POLICY_H
#include <stdint.h>
static int sfc_render_due(unsigned frame,unsigned skipped,unsigned skip,
                          int64_t lag,int64_t period,int adaptive)
{
    if(!adaptive)return frame%(skip+1)==0;
    if(!frame)return 1;
    if(skipped<skip)return 0;
    return skipped>=skip+1||lag<=period/2;
}
#endif
