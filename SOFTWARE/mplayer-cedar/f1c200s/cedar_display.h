#ifndef MPLAYER_CEDAR_DISPLAY_H
#define MPLAYER_CEDAR_DISPLAY_H
#include <stdint.h>
struct cedar_picture_geometry { int width, height, stride, left, top, right, bottom; };
int vd_cedar_picture_geometry(void *picture, struct cedar_picture_geometry *geometry);
void vd_cedar_return_picture(void *picture);
int64_t vd_cedar_picture_position(void *picture);
void vo_cedar_release_picture(void);
#endif
