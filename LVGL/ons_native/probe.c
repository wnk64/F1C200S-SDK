#define _POSIX_C_SOURCE 200809L
#include "native.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

static ons_surface *canvas;
static lv_obj_t *image;
static const char *archive, *font;
static unsigned current;
static volatile sig_atomic_t stopped;
static const char *resources[] = {"cg/sayajpgfirstedition10244.png", "cg/000.jpg", "bitmap/02.png"};
static void stop_signal(int signal) { (void)signal; stopped = 1; }
static void close_clicked(lv_event_t *event) { (void)event; stopped = 1; }
static void draw(lv_event_t *event)
{
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(event);
    const lv_area_t *area = ctx->clip_area;
    int stride = lv_area_get_width(ctx->buf_area);
    for (int y = area->y1; y <= area->y2; ++y) {
        lv_color_t *out = (lv_color_t *)ctx->buf + (y - ctx->buf_area->y1) * stride;
        int sx = y - 80;
        for (int x = area->x1; x <= area->x2; ++x) {
            uint32_t pixel = sx >= 0 && sx < 640 ? canvas->pixels[(479 - x) * 640 + sx] : 0;
            out[x - ctx->buf_area->x1].full = ((pixel >> 8) & 0xf800) | ((pixel >> 5) & 0x07e0) | ((pixel >> 3) & 31);
        }
    }
}
static int load_image(void)
{
    uint64_t begin = platform_ms(); uint8_t *data; size_t length;
    if (ons_nsa_read(archive, resources[current], &data, &length)) return -1;
    ons_surface *decoded = ons_image_decode(data, length); free(data);
    if (!decoded) return -1;
    /* Preview fitting is separate from engine geometry; script integration is pending. */
    for (int y = 0; y < canvas->h; ++y)
        for (int x = 0; x < canvas->w; ++x) {
            uint32_t checker = ((x / 24 + y / 24) & 1) ? 0xff303840 : 0xff202830;
            uint32_t p = decoded->pixels[(y * decoded->h / canvas->h) * decoded->w + x * decoded->w / canvas->w];
            canvas->pixels[y * canvas->w + x] = ons_over(p, checker);
        }
    fprintf(stderr, "Native decode %s %dx%d elapsed=%llu ms\n", resources[current], decoded->w, decoded->h,
        (unsigned long long)(platform_ms() - begin));
    ons_surface_free(decoded);
    if (ons_text_gbk(canvas, font, 24, 12, 32, "\xd6\xd0\xce\xc4", 0xffffffff) != 2) return -1;
    lv_obj_invalidate(image); lv_refr_now(NULL); return 0;
}
static void next_clicked(lv_event_t *event)
{
    (void)event; current = (current + 1) % 3;
    if (load_image()) { fprintf(stderr, "Native resource switch failed\n"); stopped = 1; }
}
static void image_clicked(lv_event_t *event)
{
    (void)event; int x, y, down; platform_touch(&x, &y, &down);
    fprintf(stderr, "Native touch portrait=%d,%d game=%d,%d\n", x, y, y - 80, 479 - x);
}
static void button(int x, const char *symbol, lv_event_cb_t callback)
{
    lv_obj_t *b = lv_btn_create(lv_scr_act()); lv_obj_set_pos(b, x, 720); lv_obj_set_size(b, 80, 80);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x24343d), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x45616f), LV_STATE_PRESSED);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_t *label = lv_label_create(b); lv_label_set_text(label, symbol);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0); lv_obj_center(label);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_add_event_cb(b, callback, LV_EVENT_CLICKED, NULL);
}
int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "usage: ons-native-probe archive.nsa default.ttf\n"); return 1; }
    archive = argv[1]; font = argv[2]; canvas = ons_surface_new(640, 480);
    if (!canvas || platform_open()) { ons_surface_free(canvas); platform_close(); return 1; }
    signal(SIGINT, stop_signal); signal(SIGTERM, stop_signal);
    image = lv_obj_create(lv_scr_act()); lv_obj_remove_style_all(image);
    lv_obj_set_size(image, 480, 800);
    lv_obj_add_event_cb(image, draw, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(image, image_clicked, LV_EVENT_CLICKED, NULL);
    button(12, LV_SYMBOL_RIGHT, next_clicked); button(388, LV_SYMBOL_CLOSE, close_clicked);
    int rc = load_image() ? 1 : 0;
    uint64_t tick = platform_ms(), begin = tick, automatic = begin;
    const char *limit = getenv("ONS_NATIVE_TEST_MS");
    while (!stopped && !rc) {
        uint64_t now = platform_ms(); lv_tick_inc(now - tick); tick = now; lv_timer_handler();
        if (limit && now - begin >= strtoul(limit, NULL, 10)) break;
        if (getenv("ONS_NATIVE_AUTO") && now - automatic >= 5000) {
            automatic = now; current = (current + 1) % 3; if (load_image()) rc = 1;
        }
        struct timespec pause = {0, 10000000}; nanosleep(&pause, NULL);
    }
    platform_close(); ons_surface_free(canvas); return rc;
}
