#include <assert.h>
#include <stddef.h>
#include <stdio.h>

enum { LV_INDEV_TYPE_POINTER = 1, LV_INDEV_TYPE_KEYPAD = 2 };
typedef struct { int unused; } lv_indev_data_t;
typedef struct lv_indev_drv_t lv_indev_drv_t;
struct lv_indev_drv_t {
    int type;
    void (*read_cb)(lv_indev_drv_t *, lv_indev_data_t *);
};
typedef struct lv_indev_t {
    lv_indev_drv_t *driver;
    struct lv_indev_t *next;
} lv_indev_t;
static lv_indev_t *head;
static lv_indev_t *lv_indev_get_next(lv_indev_t *p) { return p ? p->next : head; }
#include "media_touch.h"

static void portrait(lv_indev_drv_t *d, lv_indev_data_t *p) { (void)d; (void)p; }
static void landscape(lv_indev_drv_t *d, lv_indev_data_t *p) { (void)d; (void)p; }
static void mouse(lv_indev_drv_t *d, lv_indev_data_t *p) { (void)d; (void)p; }
static void keyboard(lv_indev_drv_t *d, lv_indev_data_t *p) { (void)d; (void)p; }
int main(void)
{
    lv_indev_drv_t td = {1, portrait}, md = {1, mouse}, kd = {2, keyboard};
    lv_indev_t t = {&td, NULL}, m = {&md, &t}, k = {&kd, &m};
    head = &k;
    assert(f1_media_bind_touch(portrait, landscape) == 0);
    assert(td.read_cb == landscape && md.read_cb == mouse && kd.read_cb == keyboard);
    assert(f1_media_bind_touch(portrait, landscape) == -1); /* No double wrapping. */
    td.read_cb = portrait; t.next = &k; m.next = NULL; head = &t;
    assert(f1_media_bind_touch(portrait, landscape) == 0);
    td.read_cb = portrait; k.next = &t; t.next = NULL; head = &k;
    lv_indev_t duplicate = {&td, &t}; k.next = &duplicate;
    assert(f1_media_bind_touch(portrait, landscape) == -1);
    assert(td.read_cb == portrait && kd.read_cb == keyboard);
    head = NULL;
    assert(f1_media_bind_touch(portrait, landscape) == -1);
    puts("PASS media touch identity/order/mouse/keyboard/missing/duplicate/double-wrap");
    return 0;
}
