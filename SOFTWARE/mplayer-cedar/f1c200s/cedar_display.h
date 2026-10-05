#ifndef MPLAYER_CEDAR_DISPLAY_H
#define MPLAYER_CEDAR_DISPLAY_H
#include <stdint.h>
void vd_cedar_return_picture(void *picture);
int64_t vd_cedar_picture_position(void *picture);
void vo_cedar_release_picture(void);
#endif
