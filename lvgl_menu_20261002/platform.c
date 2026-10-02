#define _POSIX_C_SOURCE 200809L
#include "platform.h"
#include "../ft6236_20261002/touch_orientation.h"
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/input.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

static uint8_t *fb;
static int fb_fd = -1, input_fd = -1, i2c_fd = -1;
static struct fb_var_screeninfo screen;
static struct fb_fix_screeninfo fixed;
static lv_color_t strips[2][480 * 16];
static int x, y, pressed, slot;
static uint64_t last_irq;
uint64_t platform_ms(void)
{
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
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
        if (e.type == EV_SYN && e.code == SYN_REPORT) last_irq = platform_ms();
    }
    /* Keep the verified diagnostic fallback until expander IRQ is accepted. */
    if (i2c_fd >= 0 && platform_ms() - last_irq > 500) {
        uint8_t reg = 2, report[13];
        struct i2c_msg messages[2] = {
            {.addr = 0x38, .len = 1, .buf = &reg},
            {.addr = 0x38, .flags = I2C_M_RD, .len = sizeof(report), .buf = report},
        };
        struct i2c_rdwr_ioctl_data transfer = {messages, 2};
        if (ioctl(i2c_fd, I2C_RDWR, &transfer) == 2) {
            pressed = (report[0] & 15) > 0;
            if (pressed) {
                int rx = ((report[1] & 15) << 8) | report[2];
                int ry = ((report[3] & 15) << 8) | report[4];
                ft_touch_portrait(rx, ry, &x, &y);
            }
        }
    }
    if (x < 0) x = 0;
    if (x > 479) x = 479;
    if (y < 0) y = 0;
    if (y > 799) y = 799;
    data->point.x = x; data->point.y = y;
    data->state = pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}
void platform_touch(int *tx, int *ty, int *down) { *tx = x; *ty = y; *down = pressed; }
int platform_brightness(int value)
{
    FILE *f = fopen("/sys/class/backlight/backlight/brightness", value < 0 ? "r" : "w");
    if (!f) return -1;
    int result;
    if (value < 0) result = fscanf(f, "%d", &value) == 1 ? value : -1;
    else result = value >= 0 && value <= 8 && fprintf(f, "%d\n", value) > 0 ? value : -1;
    if (fclose(f)) result = -1;
    return result;
}
int platform_open(void)
{
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0 || ioctl(fb_fd, FBIOGET_VSCREENINFO, &screen) ||
        ioctl(fb_fd, FBIOGET_FSCREENINFO, &fixed)) return -1;
    if (screen.xres != 480 || screen.yres != 800 || screen.bits_per_pixel != 32 ||
        screen.red.length != 8 || screen.green.length != 8 || screen.blue.length != 8) return -1;
    fb = mmap(NULL, fixed.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb == MAP_FAILED) { fb = NULL; return -1; }
    input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    i2c_fd = open("/dev/i2c-0", O_RDWR);
    lv_init();
    static lv_disp_draw_buf_t draw;
    lv_disp_draw_buf_init(&draw, strips[0], strips[1], 480 * 16);
    static lv_disp_drv_t display; lv_disp_drv_init(&display);
    display.hor_res = 480; display.ver_res = 800;
    display.draw_buf = &draw; display.flush_cb = flush;
    lv_disp_drv_register(&display);
    static lv_indev_drv_t pointer; lv_indev_drv_init(&pointer);
    pointer.type = LV_INDEV_TYPE_POINTER; pointer.read_cb = pointer_read;
    lv_indev_drv_register(&pointer);
    return 0;
}
void platform_close(void)
{
    if (fb) { lv_deinit(); munmap(fb, fixed.smem_len); fb = NULL; }
    if (input_fd >= 0) close(input_fd);
    if (i2c_fd >= 0) close(i2c_fd);
    if (fb_fd >= 0) close(fb_fd);
}
