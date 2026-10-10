#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/drm_warpper.h"

static unsigned ioctls;
static int fail_ioctl;
static int fake_ioctl(int fd, unsigned long request, void *argument)
{
    (void)fd; (void)request;
    struct drm_srgn_atomic_commit *commit = argument;
    assert(commit->size == 1);
    ioctls++;
    if (fail_ioctl) { errno = EIO; return -1; }
    return 0;
}
#define drmIoctl fake_ioctl
#include "../driver/drm_warpper.c"
#undef drmIoctl

static drm_warpper_queue_item_t *item(int type)
{
    drm_warpper_queue_item_t *p = calloc(1, sizeof(*p));
    assert(p); p->mount.type = type; p->on_heap = true;
    return p;
}
static void initialize(drm_warpper_t *w)
{
    memset(w, 0, sizeof(*w));
    assert(spsc_bq_init(&w->layer[0].display_queue, 4) == 0);
    assert(spsc_bq_init(&w->layer[0].free_queue, 2) == 0);
    ioctls = 0; fail_ioctl = 0;
}
static void destroy(drm_warpper_t *w, bounded_layer_t *s)
{
    bounded_layer_cleanup(s);
    drm_warpper_queue_item_t *p;
    while (spsc_bq_try_pop(&w->layer[0].display_queue, (void **)&p) == 0) free(p);
    while (spsc_bq_try_pop(&w->layer[0].free_queue, (void **)&p) == 0) free(p);
    free(w->layer[0].curr_item);
    spsc_bq_destroy(&w->layer[0].display_queue);
    spsc_bq_destroy(&w->layer[0].free_queue);
}
int main(void)
{
    for (int run = 0; run < 100; run++) {
        drm_warpper_t w; bounded_layer_t s = {0}; initialize(&w);
        drm_warpper_queue_item_t *old = item(DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_YUV);
        drm_warpper_queue_item_t *a = item(DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_YUV);
        drm_warpper_queue_item_t *b = item(DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_YUV);
        w.layer[0].curr_item = old;
        assert(spsc_bq_try_push(&w.layer[0].display_queue, a) == 0);
        assert(spsc_bq_try_push(&w.layer[0].display_queue, b) == 0);
        assert(bounded_layer_step(&w, 0, &s) == 1);
        assert(w.layer[0].curr_item == a && s.retired == old && ioctls == 1);
        assert(w.layer[0].free_queue.count == 0 && w.layer[0].display_queue.count == 1);
        assert(bounded_layer_step(&w, 0, &s) == 1);
        assert(w.layer[0].curr_item == b && s.retired == a && ioctls == 2);
        assert(w.layer[0].free_queue.count == 1);
        assert(bounded_layer_step(&w, 0, &s) == 0);
        assert(!s.retired && w.layer[0].free_queue.count == 2 && ioctls == 2);
        destroy(&w, &s);

        initialize(&w); memset(&s, 0, sizeof(s));
        old = item(DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_YUV); w.layer[0].curr_item = old;
        a = item(DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_YUV);
        assert(spsc_bq_try_push(&w.layer[0].display_queue, a) == 0);
        fail_ioctl = 1;
        assert(bounded_layer_step(&w, 0, &s) == -1);
        assert(w.layer[0].curr_item == old && s.pending == a && !s.retired);
        assert(!w.layer[0].free_queue.count && s.commit_errors == 1);
        fail_ioctl = 0;
        assert(bounded_layer_step(&w, 0, &s) == 1);
        assert(w.layer[0].curr_item == a && s.retired == old && !s.pending);
        assert(spsc_bq_try_push(&w.layer[0].free_queue, item(0)) == 0);
        assert(spsc_bq_try_push(&w.layer[0].free_queue, item(0)) == 0);
        b = item(DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_YUV);
        assert(spsc_bq_try_push(&w.layer[0].display_queue, b) == 0);
        for (int n = 0; n < 1000; n++) assert(bounded_layer_step(&w, 0, &s) == 0);
        assert(s.return_busy == 1000 && s.retired == old && ioctls == 2);
        assert(w.layer[0].display_queue.count == 1 && w.layer[0].curr_item == a);
        drm_warpper_queue_item_t *released;
        assert(spsc_bq_try_pop(&w.layer[0].free_queue, (void **)&released) == 0); free(released);
        assert(bounded_layer_step(&w, 0, &s) == 1);
        assert(w.layer[0].curr_item == b && s.retired == a);
        destroy(&w, &s);

        initialize(&w); memset(&s, 0, sizeof(s));
        w.layer[0].curr_item = item(DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_YUV);
        old = w.layer[0].curr_item;
        a = item(DRM_SRGN_ATOMIC_COMMIT_MOUNT_SET_COORD);
        assert(spsc_bq_try_push(&w.layer[0].display_queue, a) == 0);
        assert(bounded_layer_step(&w, 0, &s) == 0);
        assert(w.layer[0].curr_item == old && !s.retired && !s.pending && s.commands == 1);
        a = item(DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_YUV);
        assert(spsc_bq_try_push(&w.layer[0].display_queue, a) == 0);
        fail_ioctl = 1; assert(bounded_layer_step(&w, 0, &s) == -1);
        destroy(&w, &s); /* pending failed commit and current must each be freed once */
    }
    puts("bounded display: 100 ownership/retry/backpressure/control/cleanup runs PASS");
    return 0;
}
