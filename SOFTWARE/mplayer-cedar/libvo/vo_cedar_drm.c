/* Zero-copy Cedar MB32 output using the F1C200S private DRM ioctl. */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/mman.h>

#include <drm/drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "config.h"
#include "mp_msg.h"
#include "video_out.h"
#define NO_DRAW_FRAME
#define NO_DRAW_SLICE
#include "video_out_internal.h"

#include "f1c200s/cedar_display.h"
#include "f1c200s/cedar_geometry.h"

#define DRM_SRGN_ATOMIC_COMMIT 0x00
#define DRM_SRGN_RESET_FB_CACHE 0x01
#define DRM_SRGN_MOUNT_FB_YUV 0x01
#define DRM_SRGN_SET_YUV_VIEW 0x04
#define DRM_SRGN_SET_YUV_CENTER_CROP_VIEW 0x05
#define DRM_SRGN_SET_YUV_PITCH 0x06
#define CEDAR_PHYS_MARKER 0x53475250U

struct srgn_commit_data {
    uint32_t layer_id, type, arg0, arg1, arg2;
};
struct srgn_commit {
    uint32_t size, data;
};

#define DRM_IOCTL_SRGN_ATOMIC_COMMIT \
    DRM_IOW(DRM_COMMAND_BASE + DRM_SRGN_ATOMIC_COMMIT, struct srgn_commit)
#define DRM_IOCTL_SRGN_RESET_FB_CACHE \
    DRM_IO(DRM_COMMAND_BASE + DRM_SRGN_RESET_FB_CACHE)

static const vo_info_t info = {
    "F1C200S Cedar DRM zero-copy output", "cedar_drm", "F1C200S", "MB32 only"
};

const LIBVO_EXTERN(cedar_drm)

enum display_mode { DISPLAY_FIT, DISPLAY_STRETCH, DISPLAY_CROP, DISPLAY_NATIVE };
static int drm_fd = -1;
static int screen_w, screen_h, source_w, source_h;
static int view_x, view_y, view_w, view_h;
static enum display_mode display_mode = DISPLAY_FIT;
static void *displayed_picture;
static void *retired_picture;
static int event_fd = -1;
static int frontend_fd = -1, layout_valid;
static volatile uint32_t *frontend_regs;
static struct cedar_picture_geometry layout;
static struct cedar_native_view native_view;

static void notify_frame(void *picture)
{
    if (event_fd < 0)
        return;
    int64_t pts = vd_cedar_picture_position(picture);
    if (pts >= 0 && pts / 1000 <= UINT32_MAX) {
        uint64_t message = 7 | ((uint64_t)(pts / 1000) << 32);
        (void)send(event_fd, &message, sizeof(message), MSG_DONTWAIT | MSG_NOSIGNAL);
    }
}

void vo_cedar_release_picture(void)
{
    if (retired_picture)
        vd_cedar_return_picture(retired_picture);
    retired_picture = NULL;
    if (displayed_picture)
        vd_cedar_return_picture(displayed_picture);
    displayed_picture = NULL;
}

static uint32_t pack_size(int w, int h)
{
    return ((uint32_t)(h & 0xffff) << 16) | (uint32_t)(w & 0xffff);
}

static int submit_view(void)
{
    struct srgn_commit_data data;
    struct srgn_commit commit;
    memset(&data, 0, sizeof(data));
    data.layer_id = 0;
    data.type = DRM_SRGN_SET_YUV_VIEW;
    data.arg0 = pack_size(source_w, source_h);
    data.arg1 = pack_size(view_w, view_h);
    data.arg2 = ((uint32_t)((int16_t)view_y) << 16) | (uint16_t)((int16_t)view_x);
    commit.size = 1;
    commit.data = (uint32_t)(uintptr_t)&data;
    return drmIoctl(drm_fd, DRM_IOCTL_SRGN_ATOMIC_COMMIT, &commit);
}

static int submit_crop_view(void)
{
    int crop_w = source_w, crop_h = source_h;
    struct srgn_commit_data data;
    struct srgn_commit commit;
    if ((int64_t)source_w * screen_h > (int64_t)source_h * screen_w)
        crop_w = source_h * screen_w / screen_h;
    else
        crop_h = source_w * screen_h / screen_w;
    crop_w &= ~1;
    crop_h &= ~1;
    memset(&data, 0, sizeof(data));
    data.layer_id = 0;
    data.type = DRM_SRGN_SET_YUV_CENTER_CROP_VIEW;
    data.arg0 = pack_size(source_w, source_h);
    data.arg1 = pack_size(crop_w, crop_h);
    data.arg2 = pack_size(screen_w, screen_h);
    commit.size = 1;
    commit.data = (uint32_t)(uintptr_t)&data;
    return drmIoctl(drm_fd, DRM_IOCTL_SRGN_ATOMIC_COMMIT, &commit);
}

static int configure_native_picture(void *picture)
{
    struct cedar_picture_geometry geometry;
    if (vd_cedar_picture_geometry(picture, &geometry)) return -1;
    if (layout_valid && !memcmp(&geometry, &layout, sizeof(layout))) return 0;
    struct cedar_native_view view;
    if (cedar_native_geometry(&geometry, screen_w, screen_h, &view)) return -1;
    if (!frontend_regs) {
        frontend_fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);
        if (frontend_fd < 0) return -1;
        void *mapping = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, frontend_fd, 0x01e00000);
        if (mapping == MAP_FAILED) { close(frontend_fd); frontend_fd = -1; return -1; }
        frontend_regs = mapping;
    }
    source_w = view_w = view.width; source_h = view_h = view.height;
    view_x = view.x; view_y = view.y;
    if (submit_view()) return -1;
    if ((frontend_regs[0x4c / 4] & 0xf70) != 0x620) return -1;
    /* The existing ioctl guesses pitch from visible width; MB32 storage can be wider. */
    frontend_regs[0x40 / 4] = frontend_regs[0x44 / 4] = (view.stride - 31) * 32;
    frontend_regs[0x30 / 4] = view.y_tile;
    frontend_regs[0x34 / 4] = view.uv_tile;
    __sync_synchronize(); frontend_regs[0x04 / 4] |= 1; __sync_synchronize();
    layout = geometry; native_view = view; layout_valid = 1;
    fprintf(stderr, "[cedar_drm] native visible=%dx%d storage=%dx%d stride=%d source=%d,%d output=%dx%d at %d,%d tile=%08x/%08x base=%lu/%lu\n",
        geometry.right - geometry.left, geometry.bottom - geometry.top, geometry.width, geometry.height,
        view.stride, view.source_x, view.source_y, view.width, view.height, view.x, view.y,
        view.y_tile, view.uv_tile, (unsigned long)view.y_offset, (unsigned long)view.uv_offset);
    return 0;
}

static int query_format(uint32_t format)
{
    return format == IMGFMT_YV12 ? VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW : 0;
}

static int preinit(const char *arg)
{
    const char *events = getenv("MPLAYER_OSD_FD");
    event_fd = -1;
    if (events && !strcmp(events, "3")) {
        int type;
        socklen_t size = sizeof(type);
        if (!getsockopt(3, SOL_SOCKET, SO_TYPE, &type, &size) && type == SOCK_DGRAM)
            event_fd = 3;
    }
    if (!arg || !strcmp(arg, "fit"))
        display_mode = DISPLAY_FIT;
    else if (!strcmp(arg, "stretch"))
        display_mode = DISPLAY_STRETCH;
    else if (!strcmp(arg, "crop"))
        display_mode = DISPLAY_CROP;
    else if (!strcmp(arg, "native"))
        display_mode = DISPLAY_NATIVE;
    else
        return ENOSYS;
    drm_fd = open("/dev/dri/card0", O_RDWR);
    if (drm_fd < 0)
        return 1;
    if (drmIoctl(drm_fd, DRM_IOCTL_SRGN_RESET_FB_CACHE, NULL) < 0) {
        close(drm_fd);
        drm_fd = -1;
        return 1;
    }
    return 0;
}

static int config(uint32_t width, uint32_t height, uint32_t d_width,
                  uint32_t d_height, uint32_t fullscreen, char *title,
                  uint32_t format)
{
    drmModeRes *res;
    drmModeCrtc *crtc;
    (void)d_width; (void)d_height; (void)fullscreen; (void)title;
    if (drm_fd < 0 || format != IMGFMT_YV12)
        return 1;
    res = drmModeGetResources(drm_fd);
    if (!res || !res->count_crtcs)
        return 1;
    crtc = drmModeGetCrtc(drm_fd, res->crtcs[0]);
    if (!crtc || !crtc->mode_valid)
        return 1;
    screen_w = crtc->mode.hdisplay;
    screen_h = crtc->mode.vdisplay;
    drmModeFreeCrtc(crtc);
    drmModeFreeResources(res);
    source_w = width;
    source_h = height;
    view_x = view_y = 0;
    view_w = screen_w;
    view_h = screen_h;
    if (display_mode == DISPLAY_FIT) {
        if ((int64_t)width * screen_h > (int64_t)height * screen_w) {
            view_h = height * screen_w / width;
            view_y = (screen_h - view_h) / 2;
        } else {
            view_w = width * screen_h / height;
            view_x = (screen_w - view_w) / 2;
        }
        view_x &= ~1; view_y &= ~1; view_w &= ~1; view_h &= ~1;
    }
    if (display_mode == DISPLAY_CROP)
        return submit_crop_view() ? 1 : 0;
    if (display_mode == DISPLAY_NATIVE) {
        layout_valid = 0;
        return 0;
    }
    return submit_view() ? 1 : 0;
}

static int control(uint32_t request, void *data)
{
    struct srgn_commit_data commands[1];
    struct srgn_commit commit;
    mp_image_t *mpi;
    if (request == VOCTRL_QUERY_FORMAT)
        return query_format(*(uint32_t *)data);
    if (request != VOCTRL_DRAW_IMAGE || drm_fd < 0)
        return VO_NOTIMPL;
    mpi = data;
    if (!mpi || !mpi->priv)
        return VO_FALSE;
    if (display_mode == DISPLAY_NATIVE && configure_native_picture(mpi->priv)) {
        fprintf(stderr, "[cedar_drm] invalid or unsupported native picture layout\n");
        vd_cedar_return_picture(mpi->priv);
        return VO_FALSE;
    }
    if (retired_picture) {
        drmVBlank blank;
        memset(&blank, 0, sizeof(blank));
        blank.request.type = DRM_VBLANK_RELATIVE;
        blank.request.sequence = 1;
        if (drmWaitVBlank(drm_fd, &blank) < 0) {
            vd_cedar_return_picture(mpi->priv);
            return VO_FALSE;
        }
        vd_cedar_return_picture(retired_picture);
        retired_picture = NULL;
    }
    memset(commands, 0, sizeof(commands));
    commands[0].layer_id = 0;
    commands[0].type = DRM_SRGN_MOUNT_FB_YUV;
    commands[0].arg0 = (uint32_t)(uintptr_t)(mpi->planes[0] + (display_mode == DISPLAY_NATIVE ? native_view.y_offset : 0));
    commands[0].arg1 = (uint32_t)(uintptr_t)(mpi->planes[1] + (display_mode == DISPLAY_NATIVE ? native_view.uv_offset : 0));
    commands[0].arg2 = 0;
    commit.size = 1;
    commit.data = (uint32_t)(uintptr_t)commands;
    if (drmIoctl(drm_fd, DRM_IOCTL_SRGN_ATOMIC_COMMIT, &commit) < 0)
        return VO_FALSE;
    retired_picture = displayed_picture;
    displayed_picture = mpi->priv;
    {
        drmVBlank blank;
        memset(&blank, 0, sizeof(blank));
        blank.request.type = DRM_VBLANK_RELATIVE;
        blank.request.sequence = 1;
        if (drmWaitVBlank(drm_fd, &blank) < 0) {
            mp_msg(MSGT_VO, MSGL_ERR, "[cedar_drm] vblank wait failed\n");
            return VO_TRUE;
        }
    }
    if (retired_picture)
        vd_cedar_return_picture(retired_picture);
    retired_picture = NULL;
    notify_frame(displayed_picture);
    return VO_TRUE;
}

static void draw_osd(void) { }
static void flip_page(void) { }
static void check_events(void) { }

static void uninit(void)
{
    vo_cedar_release_picture();
    if (drm_fd >= 0)
        close(drm_fd);
    drm_fd = -1;
    event_fd = -1;
    if (frontend_regs) munmap((void *)frontend_regs, 4096);
    if (frontend_fd >= 0) close(frontend_fd);
    frontend_regs = NULL; frontend_fd = -1; layout_valid = 0;
}
