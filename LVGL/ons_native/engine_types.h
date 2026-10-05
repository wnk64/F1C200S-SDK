#ifndef ONS_ENGINE_NATIVE_TYPES_H
#define ONS_ENGINE_NATIVE_TYPES_H
#include <stdint.h>
extern "C" {
#include "native.h"
#include "sprite.h"
}
typedef struct { uint8_t r, g, b, a; } ons_color;
#define ONS_PIXELFORMAT_RGB565 0
#define ONS_PIXELFORMAT_ABGR8888 1
#define ONS_PIXELFORMAT_ARGB8888 2
#endif
