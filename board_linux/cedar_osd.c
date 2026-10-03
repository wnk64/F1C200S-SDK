#include "driver/drm_warpper.h"
#include <drm_fourcc.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

enum { OSD_WIDTH = 86, OSD_HEIGHT = 800 };
static int fb_fd = -1, event_fd = -1, restore_fd = -1;
static unsigned char *fb_pixels;
static struct fb_fix_screeninfo fb_fixed;
static struct fb_var_screeninfo fb_screen;
static buffer_object_t osd;
static uint32_t plane_id, primary_plane, primary_fb, crtc_id;
static pthread_t worker;
static atomic_int running;
static int worker_started;
static unsigned long updates;
static int osd_drm_fd;

static void visible(int show)
{
    struct drm_srgn_atomic_commit_data data = { .layer_id = 1,
        .type = DRM_SRGN_ATOMIC_COMMIT_MOUNT_SET_ALPHA, .arg0 = show ? 255 : 0 };
    struct drm_srgn_atomic_commit request = { .size = 1, .data = (uint32_t)(uintptr_t)&data };
    if (drmIoctl(osd_drm_fd, DRM_IOCTL_SRGN_ATOMIC_COMMIT, &request))
        fprintf(stderr, "OSD visibility failed: %s\n", strerror(errno));
}

int __real_drm_warpper_init(drm_warpper_t *drm);
int __real_drm_warpper_destroy(drm_warpper_t *drm);

static void copy_controls(void)
{
    for (int y = 0; y < OSD_HEIGHT; ++y) {
        const uint32_t *source = (const uint32_t *)(fb_pixels +
            (y + fb_screen.yoffset) * fb_fixed.line_length) + fb_screen.xoffset;
        uint16_t *target = (uint16_t *)(osd.vaddr + y * osd.pitch);
        for (int x = 0; x < OSD_WIDTH; ++x) {
            uint32_t pixel = source[x];
            target[x] = ((pixel >> 8) & 0xf800) | ((pixel >> 5) & 0x07e0) | ((pixel >> 3) & 0x001f);
        }
    }
    ++updates;
}

static void *update_controls(void *unused)
{
    (void)unused;
    struct pollfd event = { .fd = event_fd, .events = POLLIN };
    while (atomic_load(&running)) {
        int ready = poll(&event, 1, 100);
        if (ready < 0 && errno != EINTR) break;
        if (ready > 0 && event.revents & POLLIN) {
            uint64_t count;
            if (read(event_fd, &count, sizeof(count)) == sizeof(count)) {
                if (count == 1 || count == 2) copy_controls();
                if (count == 2 || count == 3) visible(count == 2);
            }
        }
    }
    return NULL;
}

static void release_osd(int drm_fd)
{
    atomic_store(&running, 0);
    if (worker_started) { pthread_join(worker, NULL); worker_started = 0; }
    if (plane_id && osd.fb_id) drmModeSetPlane(drm_fd, plane_id, crtc_id, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if (osd.fb_id) drmModeRmFB(drm_fd, osd.fb_id);
    if (osd.vaddr && osd.vaddr != MAP_FAILED) munmap(osd.vaddr, osd.size);
    if (osd.handle) {
        struct drm_mode_destroy_dumb destroy = { .handle = osd.handle };
        drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    }
    memset(&osd, 0, sizeof(osd));
    if (fb_pixels) { munmap(fb_pixels, fb_fixed.smem_len); fb_pixels = NULL; }
    if (fb_fd >= 0) { close(fb_fd); fb_fd = -1; }
    if (event_fd >= 0) { close(event_fd); event_fd = -1; }
}

static int prepare_osd(drm_warpper_t *drm)
{
    const char *value = getenv("F1_OSD_EVENT_FD");
    if (!value) return 0;
    char *end;
    long fd = strtol(value, &end, 10);
    if (*end || fd != 3 || drm->plane_res->count_planes < 2) return -1;
    event_fd = (int)fd;
    fb_fd = open("/dev/fb0", O_RDONLY | O_CLOEXEC);
    if (fb_fd < 0 || ioctl(fb_fd, FBIOGET_VSCREENINFO, &fb_screen) ||
        ioctl(fb_fd, FBIOGET_FSCREENINFO, &fb_fixed) || fb_screen.bits_per_pixel != 32 ||
        fb_screen.xres != 480 || fb_screen.yres != 800 ||
        (uint64_t)(fb_screen.yoffset + 800) * fb_fixed.line_length > fb_fixed.smem_len) return -1;
    fb_pixels = mmap(NULL, fb_fixed.smem_len, PROT_READ, MAP_SHARED, fb_fd, 0);
    if (fb_pixels == MAP_FAILED) { fb_pixels = NULL; return -1; }
    primary_plane = drm->plane_res->planes[0];
    plane_id = drm->plane_res->planes[1];
    crtc_id = drm->crtc_id;
    drmModePlane *primary = drmModeGetPlane(drm->fd, primary_plane);
    if (!primary) return -1;
    primary_fb = primary->fb_id;
    drmModeFreePlane(primary);
    if (!primary_fb) return -1;
    struct drm_mode_create_dumb create = { .width = OSD_WIDTH, .height = OSD_HEIGHT, .bpp = 16 };
    if (drmIoctl(drm->fd, DRM_IOCTL_MODE_CREATE_DUMB, &create)) return -1;
    osd.handle = create.handle; osd.pitch = create.pitch; osd.size = create.size;
    uint32_t handles[4] = { create.handle }, pitches[4] = { create.pitch }, offsets[4] = { 0 };
    if (drmModeAddFB2(drm->fd, OSD_WIDTH, OSD_HEIGHT, DRM_FORMAT_RGB565, handles, pitches, offsets, &osd.fb_id, 0)) return -1;
    struct drm_mode_map_dumb map = { .handle = create.handle };
    if (drmIoctl(drm->fd, DRM_IOCTL_MODE_MAP_DUMB, &map)) return -1;
    osd.vaddr = mmap(NULL, create.size, PROT_READ | PROT_WRITE, MAP_SHARED, drm->fd, map.offset);
    if (osd.vaddr == MAP_FAILED) return -1;
    copy_controls();
    if (drmModeSetPlane(drm->fd, plane_id, crtc_id, osd.fb_id, 0, 0, 0, OSD_WIDTH, OSD_HEIGHT,
                        0, 0, OSD_WIDTH << 16, OSD_HEIGHT << 16)) return -1;
    osd_drm_fd = drm->fd;
    visible(0);
    atomic_store(&running, 1);
    if (pthread_create(&worker, NULL, update_controls, NULL)) return -1;
    worker_started = 1;
    fprintf(stderr, "OSD RGB565 layer1=%u size=%u pitch=%u; video remains layer0\n", plane_id, osd.size, osd.pitch);
    return 0;
}

int __wrap_drm_warpper_init(drm_warpper_t *drm)
{
    int result = __real_drm_warpper_init(drm);
    if (result) return result;
    if (prepare_osd(drm)) {
        fprintf(stderr, "OSD initialization failed: %s\n", strerror(errno));
        release_osd(drm->fd);
        __real_drm_warpper_destroy(drm);
        return -1;
    }
    return 0;
}

int __wrap_drm_warpper_destroy(drm_warpper_t *drm)
{
    int enabled = osd.handle != 0;
    if (enabled) restore_fd = dup(drm->fd);
    release_osd(drm->fd);
    int result = __real_drm_warpper_destroy(drm);
    if (restore_fd >= 0) {
        int restored = drmModeSetPlane(restore_fd, primary_plane, crtc_id, primary_fb, 0, 0, 0, 480, 800,
                                      0, 0, 480 << 16, 800 << 16);
        fprintf(stderr, "OSD released updates=%lu; fbdev restore=%d\n", updates, restored);
        close(restore_fd); restore_fd = -1;
    }
    return result;
}
