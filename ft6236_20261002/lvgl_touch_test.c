#define _POSIX_C_SOURCE 200809L
#include "lvgl/lvgl.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/input.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

static uint8_t *fb;
static struct fb_var_screeninfo screen;
static struct fb_fix_screeninfo fixed;
static int input_fd = -1, i2c_fd = -1;
static volatile sig_atomic_t stop;
static lv_obj_t *position_label, *status_label, *marker;
static int x, y, pressed, slot;
static unsigned long irq_frames, samples, hits;
static uint64_t last_irq_ms;
static lv_color_t strips[2][480 * 16];
static uint64_t now_ms(void)
{
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
static void on_signal(int n) { (void)n; stop = 1; }
static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
{
    for (int row = area->y1; row <= area->y2; row++) {
        uint32_t *out = (uint32_t *)(fb + (row + screen.yoffset) * fixed.line_length);
        for (int col = area->x1; col <= area->x2; col++) {
            lv_color32_t c; c.full = lv_color_to32(*pixels++);
            out[col + screen.xoffset] = ((uint32_t)c.ch.red << screen.red.offset) |
                ((uint32_t)c.ch.green << screen.green.offset) |
                ((uint32_t)c.ch.blue << screen.blue.offset);
        }
    }
    lv_disp_flush_ready(driver);
}
static void pointer_read(lv_indev_drv_t *driver, lv_indev_data_t *data)
{
    (void)driver;
    struct input_event e;
    while (input_fd >= 0 && read(input_fd, &e, sizeof(e)) == sizeof(e)) {
        if (e.type == EV_ABS && e.code == ABS_MT_SLOT) slot = e.value;
        if (e.type == EV_ABS && !slot) {
            if (e.code == ABS_MT_POSITION_X || e.code == ABS_X) x = e.value;
            if (e.code == ABS_MT_POSITION_Y || e.code == ABS_Y) y = e.value;
            if (e.code == ABS_MT_TRACKING_ID) pressed = e.value >= 0;
        }
        if (e.type == EV_KEY && e.code == BTN_TOUCH) pressed = e.value != 0;
        if (e.type == EV_SYN && e.code == SYN_REPORT) { irq_frames++; last_irq_ms = now_ms(); }
    }
    /* Diagnostic fallback distinguishes touch sensing from broken IRQ delivery. */
    if (i2c_fd >= 0 && now_ms() - last_irq_ms > 500) {
        uint8_t reg = 2, report[13];
        struct i2c_msg messages[2] = {
            {.addr = 0x38, .len = 1, .buf = &reg},
            {.addr = 0x38, .flags = I2C_M_RD, .len = sizeof(report), .buf = report},
        };
        struct i2c_rdwr_ioctl_data transfer = {messages, 2};
        if (ioctl(i2c_fd, I2C_RDWR, &transfer) == 2) {
            samples++;
            pressed = (report[0] & 15) > 0;
            if (pressed) { x = ((report[1] & 15) << 8) | report[2]; y = ((report[3] & 15) << 8) | report[4]; }
        }
    }
    data->point.x = x < 0 ? 0 : x > 479 ? 479 : x;
    data->point.y = y < 0 ? 0 : y > 799 ? 799 : y;
    data->state = pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}
static lv_obj_t *text(lv_obj_t *parent, const char *s, const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent); lv_label_set_text(label, s);
    lv_obj_set_style_text_font(label, font, 0); return label;
}
static void hit(lv_event_t *event)
{
    lv_obj_t *button = lv_event_get_target(event);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x168A60), 0);
    lv_label_set_text(lv_obj_get_child(button, 0), LV_SYMBOL_OK); hits++;
}
static void build_screen(void)
{
    lv_obj_t *root = lv_scr_act();
    lv_obj_set_style_bg_color(root, lv_color_hex(0xF3F5F7), 0);
    lv_obj_set_style_text_color(root, lv_color_hex(0x202428), 0);
    lv_obj_t *title = text(root, "FT6236", &lv_font_montserrat_32);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 38);
    position_label = text(root, "X 000   Y 000", &lv_font_montserrat_24);
    lv_obj_align(position_label, LV_ALIGN_CENTER, 0, -28);
    status_label = text(root, "IRQ 0   READ 0", &lv_font_montserrat_16);
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 12);
    for (unsigned i = 0; i < 4; i++) {
        lv_obj_t *button = lv_btn_create(root);
        lv_obj_remove_style_all(button); lv_obj_set_size(button, 100, 100);
        lv_obj_set_pos(button, (i & 1) ? 352 : 28, (i & 2) ? 650 : 125);
        lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(button, lv_color_hex(0xD9E7F0), 0);
        lv_obj_set_style_radius(button, 4, 0);
        lv_obj_set_style_border_width(button, 1, 0);
        lv_obj_set_style_border_color(button, lv_color_hex(0x8DA6B7), 0);
        lv_obj_t *label = text(button, (const char *[]) {"01", "02", "03", "04"}[i], &lv_font_montserrat_24);
        lv_obj_center(label); lv_obj_add_event_cb(button, hit, LV_EVENT_CLICKED, NULL);
    }
    marker = lv_obj_create(root); lv_obj_remove_style_all(marker);
    lv_obj_set_size(marker, 14, 14); lv_obj_set_style_radius(marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(marker, lv_color_hex(0xE44743), 0);
    lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, 0);
    lv_obj_clear_flag(marker, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_flag(marker, LV_OBJ_FLAG_HIDDEN);
}
int main(void)
{
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0 || ioctl(fd, FBIOGET_VSCREENINFO, &screen) || ioctl(fd, FBIOGET_FSCREENINFO, &fixed)) return 1;
    if (screen.xres != 480 || screen.yres != 800 || screen.bits_per_pixel != 32 ||
        screen.red.length != 8 || screen.green.length != 8 || screen.blue.length != 8) return 2;
    fb = mmap(NULL, fixed.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) { close(fd); return 1; }
    input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    i2c_fd = open("/dev/i2c-0", O_RDWR);
    signal(SIGINT, on_signal); signal(SIGTERM, on_signal);
    lv_init();
    static lv_disp_draw_buf_t draw; lv_disp_draw_buf_init(&draw, strips[0], strips[1], 480 * 16);
    lv_disp_drv_t display; lv_disp_drv_init(&display);
    display.hor_res = 480; display.ver_res = 800; display.draw_buf = &draw; display.flush_cb = flush;
    lv_disp_drv_register(&display);
    lv_indev_drv_t pointer; lv_indev_drv_init(&pointer);
    pointer.type = LV_INDEV_TYPE_POINTER; pointer.read_cb = pointer_read; lv_indev_drv_register(&pointer);
    build_screen();
    uint64_t before = now_ms(), update = 0;
    while (!stop) {
        uint64_t now = now_ms(); lv_tick_inc((uint32_t)(now - before)); before = now;
        lv_timer_handler();
        if (now - update >= 100) {
            update = now; lv_label_set_text_fmt(position_label, "X %03d   Y %03d", x, y);
            lv_label_set_text_fmt(status_label, "IRQ %lu  READ %lu  HIT %lu", irq_frames, samples, hits);
            if (pressed) { lv_obj_clear_flag(marker, LV_OBJ_FLAG_HIDDEN); lv_obj_set_pos(marker, x - 7, y - 7); }
            else lv_obj_add_flag(marker, LV_OBJ_FLAG_HIDDEN);
        }
        struct timespec delay = {0, 5000000}; nanosleep(&delay, NULL);
    }
    fprintf(stderr, "touch-test exit: IRQ=%lu samples=%lu hits=%lu\n", irq_frames, samples, hits);
    lv_deinit();
    if (input_fd >= 0) close(input_fd); if (i2c_fd >= 0) close(i2c_fd);
    munmap(fb, fixed.smem_len); close(fd); return 0;
}
