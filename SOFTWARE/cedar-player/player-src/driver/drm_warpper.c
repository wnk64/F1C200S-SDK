#include <drm_fourcc.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "driver/drm_warpper.h"
#include "utils/log.h"
#include "driver/srgn_drm.h"
#include "config.h"
#include "utils/spsc_queue.h"
#include "utils/frame_gap.h"

static int64_t drm_monotonic_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static inline int DRM_IOCTL(int fd, unsigned long cmd, void *arg) {
  int ret = drmIoctl(fd, cmd, arg);
  return ret < 0 ? -errno : ret;
}

static int drm_warpper_wait_for_vsync(drm_warpper_t *drm_warpper){
    drm_warpper->blank.request.type = DRM_VBLANK_RELATIVE;
    drm_warpper->blank.request.sequence = 1;
    // log_info("wait for vsync");
    if (drmWaitVBlank(drm_warpper->fd, (drmVBlankPtr) &drm_warpper->blank)) {
      log_error("drmWaitVBlank failed");
      return -1;
    }
    return 0;
}

typedef struct {
    drm_warpper_queue_item_t *pending, *retired;
    unsigned long frames, commands, return_busy, commit_errors;
} bounded_layer_t;

/* Called only after a successful vblank. Never return a still-scanned frame. */
static int bounded_layer_step(drm_warpper_t *w, int id, bounded_layer_t *s)
{
    layer_t *layer = &w->layer[id];
    if (s->retired) {
        if (spsc_bq_try_push(&layer->free_queue, s->retired) != 0) {
            s->return_busy++;
            return 0;
        }
        s->retired = NULL;
    }
    if (!s->pending && spsc_bq_try_pop(&layer->display_queue, (void **)&s->pending) != 0)
        return 0;
    struct drm_srgn_atomic_commit_data command = s->pending->mount;
    command.layer_id = id;
    struct drm_srgn_atomic_commit request = {
        .size = 1, .data = (uint32_t)(uintptr_t)&command,
    };
    if (drmIoctl(w->fd, DRM_IOCTL_SRGN_ATOMIC_COMMIT, &request) < 0) {
        s->commit_errors++;
        return -1;
    }
    s->commands++;
    if (command.type == DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_NORMAL ||
        command.type == DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_YUV) {
        s->retired = layer->curr_item;
        layer->curr_item = s->pending;
        s->frames++;
    } else if (s->pending->on_heap) {
        free(s->pending);
    }
    s->pending = NULL;
    return command.type == DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_YUV ? 1 : 0;
}

static void bounded_layer_cleanup(bounded_layer_t *s)
{
    /* Decoder destruction owns picture buffers after the display worker joins. */
    free(s->pending); free(s->retired);
    s->pending = s->retired = NULL;
}

// 内核那边会缓存用户地址到物理地址，以便快速挂。
// 每次启动前都需要reset cache，避免上次启动的残留。
void drm_warpper_reset_cache_ioctl(drm_warpper_t *drm_warpper){
    int ret;
    ret = drmIoctl(drm_warpper->fd, DRM_IOCTL_SRGN_RESET_FB_CACHE, NULL);
    if(ret < 0){
        log_error("drm_warpper_reset_cache_ioctl failed %s(%d)", strerror(errno), errno);
    }
}

// static void drm_warpper_switch_buffer_ioctl(drm_warpper_t *drm_warpper,int layer_id,int type,uint8_t *ch0_addr,uint8_t *ch1_addr,uint8_t *ch2_addr){
//     int ret;
//     struct drm_srgn_mount_fb srgn_mount_fb;

//     srgn_mount_fb.layer_id = layer_id;
//     srgn_mount_fb.type = type;
//     srgn_mount_fb.ch0_addr = (uint32_t)ch0_addr;
//     srgn_mount_fb.ch1_addr = (uint32_t)ch1_addr;
//     srgn_mount_fb.ch2_addr = (uint32_t)ch2_addr;
    
//     ret = drmIoctl(drm_warpper->fd, DRM_IOCTL_SRGN_MOUNT_FB, &srgn_mount_fb);
//     if(ret < 0){
//         log_error("drm_warpper_switch_buffer_ioctl failed %s(%d)", strerror(errno), errno);
//     }
// }

static void* drm_warpper_display_thread(void *arg){
    drm_warpper_t *drm_warpper = (drm_warpper_t *)arg;
    struct drm_srgn_atomic_commit_data commits[12] = {0};
    struct drm_srgn_atomic_commit commit_req = {
        .size = 0,
        .data = (uint32_t)(uintptr_t)commits,
    };
    int ret;

    log_info("==> DRM_Warpper Display Thread Started!");
    const char *stats_env = getenv("CEDAR_FRAME_GAP_STATS");
    int stats_enabled = stats_env && strcmp(stats_env, "1") == 0;
    frame_gap_t gaps = {0};
    unsigned long commit_errors = 0;
    const char *bounded_env = getenv("CEDAR_DISPLAY_BOUNDED");
    int bounded = bounded_env && strcmp(bounded_env, "1") == 0;
    bounded_layer_t bounded_layers[4] = {{0}};
    if (bounded) log_info("bounded display: one command/layer/vblank, deferred nonblocking returns");
    if (stats_enabled) gaps.report_us = drm_monotonic_us();

    while(atomic_load(&drm_warpper->thread_running)){
        int vsync_result = drm_warpper_wait_for_vsync(drm_warpper);
        if (bounded) {
            if (vsync_result < 0) { usleep(2000); continue; }
            for (int i = 0; i < 4; i++) if (drm_warpper->layer[i].used) {
                int mounted = bounded_layer_step(drm_warpper, i, &bounded_layers[i]);
                if (i == DRM_WARPPER_LAYER_VIDEO) {
                    if (mounted > 0 && stats_enabled) frame_gap_success(&gaps, drm_monotonic_us());
                    if (mounted < 0) commit_errors++;
                }
            }
            goto report_stats;
        }
        int video_mount = 0;
        // log_info("vsync");
        commit_req.size = 0;
        for(int i = 0; i < 4; i++){
            layer_t* layer = &drm_warpper->layer[i];
            if(layer->used){
                drm_warpper_queue_item_t* item;
                while(spsc_bq_try_pop(&layer->display_queue, (void**)&item) == 0){
                    // somthing is wait to be displayed.
                    // so, switch buffer using ioctl,and put current item to free queue.
                    // log_info("switch buffer on layer %d type %d", i, item->mount.type);
                    commits[commit_req.size].layer_id = i;
                    commits[commit_req.size].type = item->mount.type;
                    commits[commit_req.size].arg0 = item->mount.arg0;
                    commits[commit_req.size].arg1 = item->mount.arg1;
                    commits[commit_req.size].arg2 = item->mount.arg2;
                    if (i == DRM_WARPPER_LAYER_VIDEO &&
                        item->mount.type == DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_YUV)
                        video_mount = 1;
                    commit_req.size++;
                    if(item->mount.type == DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_NORMAL 
                        || item->mount.type == DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_YUV){
                        if(layer->curr_item){
                            // 如果程序终止 对端consumer可能已经退出 导致这里卡死。
                            if (spsc_bq_push(&layer->free_queue, layer->curr_item) != 0)
                                free(layer->curr_item);
                        }
                        layer->curr_item = item;
                    }
                    else{
                        if(item->on_heap){
                            free(item);
                        }
                    }
                }

            }
        }
        if(commit_req.size > 0){
            ret = drmIoctl(drm_warpper->fd, DRM_IOCTL_SRGN_ATOMIC_COMMIT, &commit_req);
            if(ret < 0){
                log_error("DRM_IOCTL_SRGN_ATOMIC_COMMIT failed %s(%d)", strerror(errno), errno);
                if (stats_enabled && video_mount) commit_errors++;
            } else if (stats_enabled && video_mount) {
                frame_gap_success(&gaps, drm_monotonic_us());
            }
        }
report_stats:
        if (stats_enabled) {
            int64_t now = drm_monotonic_us();
            if (now - gaps.report_us >= 5000000) {
                log_info("video commits: total=%llu window=%llu span_us=%lld max_gap_us=%llu lifetime_max_us=%llu over50=%llu over100=%llu over250=%llu idle_us=%llu errors=%lu",
                         (unsigned long long)gaps.total,
                         (unsigned long long)gaps.window_frames,
                         (long long)(now - gaps.report_us),
                         (unsigned long long)gaps.max_gap_us,
                         (unsigned long long)gaps.lifetime_max_gap_us,
                         (unsigned long long)gaps.over50,
                         (unsigned long long)gaps.over100,
                         (unsigned long long)gaps.over250,
                         (unsigned long long)frame_gap_idle(&gaps, now),
                         commit_errors);
                frame_gap_reset_window(&gaps, now);
                if (bounded) {
                    bounded_layer_t *s = &bounded_layers[DRM_WARPPER_LAYER_VIDEO];
                    log_info("bounded video totals: frames=%lu commands=%lu return_busy=%lu commit_errors=%lu pending=%d retired=%d",
                             s->frames, s->commands, s->return_busy, s->commit_errors,
                             s->pending != NULL, s->retired != NULL);
                }
            }
        }
    }

    for (int i = 0; i < 4; i++) bounded_layer_cleanup(&bounded_layers[i]);
    log_info("==> DRM_Warpper Display Thread Ended!");
    return NULL;
}

int drm_warpper_enqueue_display_item(drm_warpper_t *drm_warpper,int layer_id,drm_warpper_queue_item_t* item){
    layer_t* layer = &drm_warpper->layer[layer_id];
    return spsc_bq_push(&layer->display_queue, item);
}

int drm_warpper_try_enqueue_display_item(drm_warpper_t *drm_warpper,int layer_id,drm_warpper_queue_item_t* item){
    layer_t* layer = &drm_warpper->layer[layer_id];
    return spsc_bq_try_push(&layer->display_queue, item);
}

int drm_warpper_dequeue_free_item(drm_warpper_t *drm_warpper,int layer_id,drm_warpper_queue_item_t** out_item){
    layer_t* layer = &drm_warpper->layer[layer_id];
    return spsc_bq_pop(&layer->free_queue, (void**)out_item);
}

int drm_warpper_try_dequeue_free_item(drm_warpper_t *drm_warpper,int layer_id,drm_warpper_queue_item_t** out_item){
    layer_t* layer = &drm_warpper->layer[layer_id];
    return spsc_bq_try_pop(&layer->free_queue, (void**)out_item);
}

int drm_warpper_set_layer_coord(drm_warpper_t *drm_warpper,int layer_id,int x,int y){
    drm_warpper_queue_item_t *item = malloc(sizeof(drm_warpper_queue_item_t));
    if(item == NULL){
        log_error("failed to allocate memory");
        return -1;
    }
    item->mount.layer_id = layer_id;
    item->mount.type = DRM_SRGN_ATOMIC_COMMIT_MOUNT_SET_COORD;
    item->mount.arg0 = (int16_t)y << 16 | (int16_t)x;
    item->mount.arg1 = 0;
    item->mount.arg2 = 0;
    item->userdata = NULL;
    item->on_heap = true;
#ifndef APP_RELEASE
    log_trace("drm coord y:%d,x:%d,reg:%x",y,x,item->mount.arg0);
#endif // APP_RELEASE
    return drm_warpper_enqueue_display_item(drm_warpper, layer_id, item);
}

int drm_warpper_set_layer_alpha(drm_warpper_t *drm_warpper,int layer_id,int alpha){
    drm_warpper_queue_item_t *item = malloc(sizeof(drm_warpper_queue_item_t));
    if(item == NULL){
        log_error("failed to allocate memory");
        return -1;
    }
    item->mount.layer_id = layer_id;
    item->mount.type = DRM_SRGN_ATOMIC_COMMIT_MOUNT_SET_ALPHA;
    item->mount.arg0 = alpha;
    item->mount.arg1 = 0;
    item->mount.arg2 = 0;
    item->userdata = NULL;
    item->on_heap = true;
    return drm_warpper_enqueue_display_item(drm_warpper, layer_id, item);
}

int drm_warpper_set_yuv_view(drm_warpper_t *drm_warpper,int layer_id,
                             int src_w,int src_h,int out_w,int out_h,
                             int x,int y){
    drm_warpper_queue_item_t *item = malloc(sizeof(drm_warpper_queue_item_t));
    if(item == NULL){
        log_error("failed to allocate memory");
        return -1;
    }
    item->mount.layer_id = layer_id;
    item->mount.type = DRM_SRGN_ATOMIC_COMMIT_MOUNT_SET_YUV_VIEW;
    item->mount.arg0 = ((uint32_t)(src_h & 0xffff) << 16) | (uint32_t)(src_w & 0xffff);
    item->mount.arg1 = ((uint32_t)(out_h & 0xffff) << 16) | (uint32_t)(out_w & 0xffff);
    item->mount.arg2 = ((uint32_t)((int16_t)y) << 16) | (uint16_t)((int16_t)x);
    item->userdata = NULL;
    item->on_heap = true;
    log_info("set yuv view src=%dx%d out=%dx%d coord=%d,%d", src_w, src_h, out_w, out_h, x, y);
    return drm_warpper_enqueue_display_item(drm_warpper, layer_id, item);
}

int drm_warpper_init(drm_warpper_t *drm_warpper){
    int ret;

    memset(drm_warpper, 0, sizeof(drm_warpper_t));

    drm_warpper->fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (drm_warpper->fd < 0) {
        log_error("open /dev/dri/card0 failed");
        return -1;
    }

    ret = drmSetClientCap(drm_warpper->fd, DRM_CLIENT_CAP_ATOMIC, 1);
    if(ret) {
        log_error("No atomic modesetting support: %s", strerror(errno));
        close(drm_warpper->fd);
        return -1;
    }

    drm_warpper->res = drmModeGetResources(drm_warpper->fd);
    if (!drm_warpper->res || drm_warpper->res->count_crtcs == 0 || drm_warpper->res->count_connectors == 0) {
        log_error("drmModeGetResources failed or no CRTCs/connectors");
        close(drm_warpper->fd);
        return -1;
    }
    drm_warpper->crtc_id = drm_warpper->res->crtcs[0];
    drm_warpper->conn_id = drm_warpper->res->connectors[0];

    ret = drmSetClientCap(drm_warpper->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    if (ret) {
      log_error("failed to set client cap\n");
      drmModeFreeResources(drm_warpper->res);
      close(drm_warpper->fd);
      return -1;
    }
    drm_warpper->plane_res = drmModeGetPlaneResources(drm_warpper->fd);
    if (!drm_warpper->plane_res) {
        log_error("drmModeGetPlaneResources failed");
        drmModeFreeResources(drm_warpper->res);
        close(drm_warpper->fd);
        return -1;
    }
    log_info("Available Plane Count: %d", drm_warpper->plane_res->count_planes);

    drm_warpper->conn = drmModeGetConnector(drm_warpper->fd, drm_warpper->conn_id);
    if (!drm_warpper->conn) {
        log_error("drmModeGetConnector failed");
        drmModeFreePlaneResources(drm_warpper->plane_res);
        drmModeFreeResources(drm_warpper->res);
        close(drm_warpper->fd);
        return -1;
    }

    log_info("Connector Name: %s, %dx%d, Refresh Rate: %d",
        drm_warpper->conn->modes[0].name, drm_warpper->conn->modes[0].vdisplay, drm_warpper->conn->modes[0].hdisplay,
        drm_warpper->conn->modes[0].vrefresh);

    /* DRM stores this portrait mode as hdisplay=480, vdisplay=800. */
    drm_warpper->display_width = drm_warpper->conn->modes[0].hdisplay;
    drm_warpper->display_height = drm_warpper->conn->modes[0].vdisplay;

    drm_warpper->blank.request.type = DRM_VBLANK_RELATIVE;
    drm_warpper->blank.request.sequence = 1;

    drm_warpper_reset_cache_ioctl(drm_warpper);

    atomic_store(&drm_warpper->thread_running, 1);
    if (pthread_create(&drm_warpper->display_thread, NULL, drm_warpper_display_thread, drm_warpper) != 0) {
        log_error("Failed to create display thread");
        drmModeFreeConnector(drm_warpper->conn);
        drmModeFreePlaneResources(drm_warpper->plane_res);
        drmModeFreeResources(drm_warpper->res);
        close(drm_warpper->fd);
        return -1;
    }
    return 0;
}

int drm_warpper_destroy(drm_warpper_t *drm_warpper){
    int was_running = atomic_exchange(&drm_warpper->thread_running, 0);
    /* Wake a worker blocked returning frames before waiting for it. */
    for(int i = 0; i < 4; i++){
        layer_t *layer = &drm_warpper->layer[i];
        if (layer->used) {
            spsc_bq_close(&layer->display_queue);
            spsc_bq_close(&layer->free_queue);
        }
    }
    if (was_running) {
        log_info("wait for display thread to finish");
        pthread_join(drm_warpper->display_thread, NULL);
        log_info("display thread finished");
    }
    for(int i = 0; i < 4; i++){
        drm_warpper_destroy_layer(drm_warpper, i);
    }
    if (drm_warpper->conn)
        drmModeFreeConnector(drm_warpper->conn);
    if (drm_warpper->plane_res)
        drmModeFreePlaneResources(drm_warpper->plane_res);
    if (drm_warpper->res)
        drmModeFreeResources(drm_warpper->res);
    drm_warpper->conn = NULL;
    drm_warpper->plane_res = NULL;
    drm_warpper->res = NULL;
    if (drm_warpper->fd >= 0)
        close(drm_warpper->fd);
    drm_warpper->fd = -1;

    return 0;
}

static int drm_warpper_create_buffer_object(int fd,buffer_object_t* bo,int width,int height,drm_warpper_layer_mode_t mode){
    struct drm_mode_create_dumb creq;
    struct drm_mode_map_dumb mreq;
    uint32_t handles[4], pitches[4], offsets[4];
    uint64_t modifiers[4];
    int ret;
 
    memset(&creq, 0, sizeof(struct drm_mode_create_dumb));
    if(mode == DRM_WARPPER_LAYER_MODE_MB32_NV12){
        creq.width = width;
        creq.height = height * 3 / 2;
        creq.bpp = 8;
    }
    else if(mode == DRM_WARPPER_LAYER_MODE_RGB565){
        creq.width = width;
        creq.height = height;
        creq.bpp = 16;
    }
    else if(mode == DRM_WARPPER_LAYER_MODE_ARGB8888){
        creq.width = width;
        creq.height = height;
        creq.bpp = 32;
    }
    else{
        log_error("invalid layer mode");
        return -1;
    }


    ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
    if (ret < 0) {
      log_error("cannot create dumb buffer (%d): %m", errno);
      return -errno;
    }
  
    memset(&offsets, 0, sizeof(offsets));
    memset(&handles, 0, sizeof(handles));
    memset(&pitches, 0, sizeof(pitches));
    memset(&modifiers, 0, sizeof(modifiers));

    if(mode == DRM_WARPPER_LAYER_MODE_MB32_NV12){
        offsets[0] = 0;
        handles[0] = creq.handle;
        pitches[0] = creq.pitch;
        modifiers[0] = DRM_FORMAT_MOD_ALLWINNER_TILED;
      
        offsets[1] = creq.pitch * height;
        handles[1] = creq.handle;
        pitches[1] = creq.pitch;
        modifiers[1] = DRM_FORMAT_MOD_ALLWINNER_TILED;
    }
    else{
        offsets[0] = 0;
        handles[0] = creq.handle;
        pitches[0] = creq.pitch;
        modifiers[0] = 0;
    }

    if(mode == DRM_WARPPER_LAYER_MODE_MB32_NV12){
        ret = drmModeAddFB2WithModifiers(fd, width, height, DRM_FORMAT_NV12, handles,
                                     pitches, offsets, modifiers, &bo->fb_id,
                                     DRM_MODE_FB_MODIFIERS);
    }
    else if(mode == DRM_WARPPER_LAYER_MODE_RGB565){
        ret = drmModeAddFB2(fd, width, height, DRM_FORMAT_RGB565, handles, pitches, offsets,&bo->fb_id, 0);
    }
    else if(mode == DRM_WARPPER_LAYER_MODE_ARGB8888){
        ret = drmModeAddFB2(fd, width, height, DRM_FORMAT_ARGB8888, handles, pitches, offsets,&bo->fb_id, 0);
    }
  
    if (ret) {
      log_error("drmModeAddFB2 return err %d", ret);
      return -1;
    }
    
    /* prepare buffer for memory mapping */
    memset(&mreq, 0, sizeof(mreq));
    mreq.handle = creq.handle;
    ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
    if (ret) {
      log_error("1st cannot map dumb buffer (%d): %m\n", errno);
      return -1;
    }
    /* perform actual memory mapping */
    bo->vaddr = mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);   

    if (bo->vaddr == MAP_FAILED) {
        log_error("2nd cannot mmap dumb buffer (%d): %m\n", errno);
      return -1;
    }

    return 0;
}


int drm_warpper_init_layer(drm_warpper_t *drm_warpper,int layer_id,int width,int height,drm_warpper_layer_mode_t mode){

    layer_t* layer = &drm_warpper->layer[layer_id];
    int ret;

    ret = spsc_bq_init(&layer->display_queue, 4);
    if(ret < 0){
        log_error("failed to initialize display queue");
        return -1;
    }
    ret = spsc_bq_init(&layer->free_queue, 2);
    if(ret < 0){
        log_error("failed to initialize free queue");
        return -1;
    }

    layer->mode = mode;
    layer->used = true;
    layer->width = width;
    layer->height = height;

    layer->curr_item = NULL;

    return 0;
}

int drm_warpper_destroy_layer(drm_warpper_t *drm_warpper,int layer_id){
    layer_t* layer = &drm_warpper->layer[layer_id];
    if(!layer->used){
        return 0;
    }
    /* Producers and the display worker must be joined before destroying a layer. */
    drm_warpper_queue_item_t *item;
    while (spsc_bq_try_pop(&layer->display_queue, (void **)&item) == 0)
        free(item);
    while (spsc_bq_try_pop(&layer->free_queue, (void **)&item) == 0)
        free(item);
    free(layer->curr_item);
    layer->curr_item = NULL;
    spsc_bq_destroy(&layer->display_queue);
    spsc_bq_destroy(&layer->free_queue);
    layer->used = false;
    return 0;
}

int drm_warpper_allocate_buffer(drm_warpper_t *drm_warpper,int layer_id,buffer_object_t *buf){
    int ret;
    layer_t* layer = &drm_warpper->layer[layer_id];
    buf->width = layer->width;
    buf->height = layer->height;
    ret = drm_warpper_create_buffer_object(drm_warpper->fd, buf, layer->width, layer->height, layer->mode);
    if(ret < 0){
        log_error("failed to allocate buffer");
        return -1;
    }
    return 0;
}

int drm_warpper_free_buffer(drm_warpper_t *drm_warpper,int layer_id,buffer_object_t *buf){
    struct drm_mode_destroy_dumb destroy;

    memset(&destroy, 0, sizeof(struct drm_mode_destroy_dumb));

    drmModeRmFB(drm_warpper->fd, buf->fb_id);
    munmap(buf->vaddr, buf->size);

    destroy.handle = buf->handle;
    drmIoctl(drm_warpper->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);

    return 0;
}



int drm_warpper_mount_layer(drm_warpper_t *drm_warpper,int layer_id,int x,int y,buffer_object_t *buf){
    int ret;
    ret = drmModeSetPlane(drm_warpper->fd, 
        drm_warpper->plane_res->planes[layer_id], 
        drm_warpper->crtc_id, 
        buf->fb_id, 
        0,
        x, y, 
        buf->width, buf->height, 
        0, 0,
        (buf->width) << 16, (buf->height) << 16
    );
    if (ret < 0)
        log_error("drmModeSetPlane err %d", ret);
    return 0;
}
