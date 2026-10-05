#ifndef MENU_PLATFORM_H
#define MENU_PLATFORM_H
#include "lvgl/lvgl.h"
#include <stdint.h>
int platform_open(void);
void platform_close(void);
uint64_t platform_ms(void);
void platform_touch(int *x, int *y, int *pressed);
int platform_brightness(int value);
#endif
