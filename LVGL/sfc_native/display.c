#include "../menu/platform.c"
#include "blit.h"
int sfc_present(const uint16_t *pixels, unsigned width, unsigned height, unsigned pitch)
{
    if (screen.xoffset || screen.yoffset || screen.red.offset != 16 || screen.green.offset != 8 || screen.blue.offset != 0)
        return -1;
    return sfc_blit(fb, fixed.smem_len, fixed.line_length, pixels, width, height, pitch);
}
