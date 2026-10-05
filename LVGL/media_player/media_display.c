#define _GNU_SOURCE
#include "platform.h"
void portrait_platform_close(void);
#define platform_open portrait_platform_open
#define platform_close portrait_platform_close
#include "../menu/platform.c"
#undef platform_open
#undef platform_close
#include "media_display.h"
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

static lv_color_t landscape_strips[2][800 * 16];
static lv_disp_drv_t landscape_driver;
static lv_disp_draw_buf_t landscape_draw;
static int dirty;
static int drm_fd = -1;
static uint32_t osd_handle, osd_fb, osd_plane, primary_plane, primary_fb, crtc;
static unsigned osd_pitch;
static uint64_t osd_size;
static unsigned char *osd_pixels;
struct private_item { uint32_t layer, type, arg0, arg1, arg2; };
struct private_commit { uint32_t count, data; };
#define PRIVATE_COMMIT DRM_IOW(DRM_COMMAND_BASE, struct private_commit)
static void landscape_flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
{
    for (int row = area->y1; row <= area->y2; ++row)
        for (int col = area->x1; col <= area->x2; ++col) {
            lv_color32_t color; color.full = lv_color_to32(*pixels++);
            uint32_t *output = (uint32_t *)(fb + (col + screen.yoffset) * fixed.line_length);
            output[479 - row + screen.xoffset] = ((uint32_t)color.ch.red << screen.red.offset) |
                ((uint32_t)color.ch.green << screen.green.offset) | ((uint32_t)color.ch.blue << screen.blue.offset);
        }
    if (area->y2 >= 394) dirty = 1;
    lv_disp_flush_ready(driver);
}
static void landscape_pointer(lv_indev_drv_t *driver, lv_indev_data_t *data)
{
    pointer_read(driver, data);
    int px = data->point.x;
    data->point.x = data->point.y;
    data->point.y = 479 - px;
}
int platform_open(void)
{
    int rc = portrait_platform_open(); if (rc) return rc;
    int descriptors[] = {fb_fd, input_fd, i2c_fd};
    for (unsigned i = 0; i < sizeof(descriptors) / sizeof(*descriptors); ++i)
        if (descriptors[i] >= 0) fcntl(descriptors[i], F_SETFD, FD_CLOEXEC);
    lv_disp_t *display = lv_disp_get_default(); landscape_driver = *display->driver;
    lv_disp_draw_buf_init(&landscape_draw, landscape_strips[0], landscape_strips[1], 800 * 16);
    landscape_driver.hor_res = 800; landscape_driver.ver_res = 480;
    landscape_driver.draw_buf = &landscape_draw; landscape_driver.flush_cb = landscape_flush;
    lv_disp_drv_update(display, &landscape_driver);
    lv_indev_t *input = lv_indev_get_next(NULL);
    if (input) input->driver->read_cb = landscape_pointer;
    return 0;
}
void platform_close(void)
{
    portrait_platform_close(); fb_fd = input_fd = i2c_fd = -1;
}
int media_osd_open(void)
{
    drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC); if (drm_fd < 0) return -1;
    drmModeRes *resources = drmModeGetResources(drm_fd);
    drmModePlaneRes *planes = drmModeGetPlaneResources(drm_fd);
    if (!resources || !resources->count_crtcs || !planes || planes->count_planes < 2) {
        if (resources) drmModeFreeResources(resources);
        if (planes) drmModeFreePlaneResources(planes);
        media_osd_close(); return -1;
    }
    crtc = resources->crtcs[0]; primary_plane = planes->planes[0]; osd_plane = planes->planes[1];
    drmModeFreeResources(resources); drmModeFreePlaneResources(planes);
    drmModePlane *primary = drmModeGetPlane(drm_fd, primary_plane);
    if (!primary) { media_osd_close(); return -1; }
    primary_fb = primary->fb_id; drmModeFreePlane(primary);
    struct drm_mode_create_dumb create = {.width = 86, .height = 800, .bpp = 16};
    if (drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create)) goto fail;
    osd_handle = create.handle; osd_pitch = create.pitch; osd_size = create.size;
    uint32_t handles[4] = {osd_handle}, pitches[4] = {osd_pitch}, offsets[4] = {0};
    if (drmModeAddFB2(drm_fd, 86, 800, DRM_FORMAT_RGB565, handles, pitches, offsets, &osd_fb, 0)) goto fail;
    struct drm_mode_map_dumb mapping = {.handle = osd_handle};
    if (drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &mapping)) goto fail;
    osd_pixels = mmap(NULL, osd_size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd, mapping.offset);
    if (osd_pixels == MAP_FAILED) { osd_pixels = NULL; goto fail; }
    memset(osd_pixels, 0, osd_size);
    if (drmModeSetPlane(drm_fd, osd_plane, crtc, osd_fb, 0, 0, 0, 86, 800, 0, 0, 86 << 16, 800 << 16)) goto fail;
    dirty = 1; media_osd_present(0);
    fprintf(stderr, "MPLAYER UI OSD: layer1 buffer=%llu bytes pitch=%u\n", (unsigned long long)osd_size, osd_pitch);
    return 0;
fail:
    media_osd_close(); return -1;
}
void media_osd_present(int visible)
{
    if (drm_fd < 0 || !osd_pixels) return;
    if (dirty) {
        for (int y = 0; y < 800; ++y) {
            uint32_t *source = (uint32_t *)(fb + (y + screen.yoffset) * fixed.line_length) + screen.xoffset;
            uint16_t *output = (uint16_t *)(osd_pixels + y * osd_pitch);
            for (int x = 0; x < 86; ++x) {
                uint32_t pixel = source[x];
                output[x] = ((pixel >> 8) & 0xf800) | ((pixel >> 5) & 0x07e0) | ((pixel >> 3) & 31);
            }
        }
        dirty = 0;
    }
    struct private_item items[2] = {{1, 3, 255, 0, 0}, {1, 2, visible ? 0 : 480, 0, 0}};
    struct private_commit commit = {2, (uint32_t)(uintptr_t)items};
    if (drmIoctl(drm_fd, PRIVATE_COMMIT, &commit)) perror("MPlayer OSD visibility");
}
void media_osd_close(void)
{
    if (drm_fd < 0) return;
    if (osd_fb) drmModeSetPlane(drm_fd, osd_plane, crtc, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if (osd_fb) drmModeRmFB(drm_fd, osd_fb);
    if (osd_pixels) munmap(osd_pixels, osd_size);
    if (osd_handle) {
        struct drm_mode_destroy_dumb destroy = {.handle = osd_handle};
        drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    }
    if (primary_fb) drmModeSetPlane(drm_fd, primary_plane, crtc, primary_fb, 0, 0, 0, 480, 800, 0, 0, 480 << 16, 800 << 16);
    close(drm_fd); drm_fd = -1; osd_pixels = NULL; osd_fb = osd_handle = primary_fb = 0;
}
