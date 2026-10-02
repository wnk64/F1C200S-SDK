#ifndef WILIWILI_LV_CONF_H
#define WILIWILI_LV_CONF_H

#ifdef WILIWILI_BOARD
#define LV_COLOR_DEPTH 16
#define LV_MEM_SIZE (192U * 1024U)
#else
#ifndef LV_COLOR_DEPTH
#define LV_COLOR_DEPTH 32
#endif
#define LV_MEM_SIZE (512U * 1024U)
#endif
#define LV_USE_LOG 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1

/* Windows migration host: render real upstream Chinese metadata. */
#ifdef WILIWILI_BOARD
#define LV_USE_FREETYPE 0
#else
#define LV_USE_FREETYPE 1
#define LV_FREETYPE_CACHE_SIZE (256U * 1024U)
#endif

/* Keep optional LVGL decoders out of the board build; thumbnails are decoded
 * by the platform adapter and video never enters the LVGL image pipeline. */
#define LV_USE_PNG 0
#define LV_USE_BMP 0
#define LV_USE_SJPG 0
#define LV_USE_GIF 0
#define LV_USE_FFMPEG 0
#define LV_USE_RLOTTIE 0

#ifndef LV_USE_QRCODE
#define LV_USE_QRCODE 1
#endif

#endif
