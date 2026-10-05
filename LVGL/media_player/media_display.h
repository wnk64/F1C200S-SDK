#ifndef MEDIA_DISPLAY_H
#define MEDIA_DISPLAY_H
#include "platform.h"
int media_osd_open(void);
void media_osd_present(int visible);
void media_osd_close(void);
#endif
