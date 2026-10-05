#include "content.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef union { size_t size; long double align; void *pointer; } allocation;
static size_t live, peak;
#define BUDGET (12u * 1024 * 1024)
static void *bounded_malloc(size_t size)
{
    if (size > BUDGET || live > BUDGET - size) return NULL;
    allocation *a = malloc(sizeof(*a) + size); if (!a) return NULL;
    a->size = size; live += size; if (live > peak) peak = live; return a + 1;
}
static void bounded_free(void *pointer)
{
    if (!pointer) return;
    allocation *a = (allocation *)pointer - 1;
    live -= a->size; free(a);
}
static void *bounded_realloc(void *pointer, size_t size)
{
    if (!pointer) return bounded_malloc(size);
    allocation *a = (allocation *)pointer - 1; size_t old = a->size;
    if (size > BUDGET || live - old > BUDGET - size) return NULL;
    allocation *next = realloc(a, sizeof(*a) + size); if (!next) return NULL;
    next->size = size; live = live - old + size; if (live > peak) peak = live; return next + 1;
}
#define STBI_MALLOC bounded_malloc
#define STBI_REALLOC bounded_realloc
#define STBI_FREE bounded_free
#define STBI_MAX_DIMENSIONS 4096
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
static char error[160], info[96];
const char *image_error(void) { return error; }
const char *image_info(void) { return info; }
uint16_t *image_open(const char *path, int native, unsigned *width, unsigned *height)
{
    int w, h, channels; peak = live; error[0] = 0;
    if (!stbi_info(path, &w, &h, &channels)) { snprintf(error, sizeof(error), "无法识别图片"); return NULL; }
    if (w < 1 || h < 1 || w > 4096 || h > 4096 || (uint64_t)w * h > 2000000) {
        snprintf(error, sizeof(error), "图片超过限制：最多200万像素"); return NULL;
    }
    unsigned char *source = stbi_load(path, &w, &h, &channels, 3);
    if (!source) { snprintf(error, sizeof(error), "解码失败或内存不足"); return NULL; }
    unsigned out_w = w, out_h = h;
    if (native) { if (out_w > 480) out_w = 480; if (out_h > 600) out_h = 600; }
    else if (w > 480 || h > 600) {
        if ((int64_t)w * 600 > (int64_t)h * 480) { out_w = 480; out_h = (uint64_t)h * 480 / w; }
        else { out_h = 600; out_w = (uint64_t)w * 600 / h; }
        if (!out_w) out_w = 1;
        if (!out_h) out_h = 1;
    }
    uint16_t *pixels = malloc((size_t)out_w * out_h * 2);
    if (!pixels) { stbi_image_free(source); snprintf(error, sizeof(error), "内存不足"); return NULL; }
    for (unsigned y = 0; y < out_h; ++y) for (unsigned x = 0; x < out_w; ++x) {
        unsigned sx = native ? x + (w - out_w) / 2 : (uint64_t)x * w / out_w;
        unsigned sy = native ? y + (h - out_h) / 2 : (uint64_t)y * h / out_h;
        const unsigned char *rgb = source + ((size_t)sy * w + sx) * 3;
        pixels[(size_t)y * out_w + x] = ((rgb[0] & 0xf8) << 8) | ((rgb[1] & 0xfc) << 3) | (rgb[2] >> 3);
    }
    stbi_image_free(source); *width = out_w; *height = out_h;
    snprintf(info, sizeof(info), "%d x %d  %s", w, h, native ? "1:1" : "Fit");
    fprintf(stderr, "READER image=%dx%d output=%ux%u decoder_peak=%lu live=%lu\n",w,h,out_w,out_h,(unsigned long)peak,(unsigned long)live);
    return pixels;
}
