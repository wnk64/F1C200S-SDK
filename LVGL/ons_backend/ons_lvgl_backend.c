#define _GNU_SOURCE
#include <SDL/SDL.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "platform.h"

static SDL_Surface *surface;
static int opened, quit_requested, old_down, old_x, old_y, mouse_captured;
static lv_obj_t *canvas;
static uint64_t tick, start;
static void stop(int signal_number) { (void)signal_number; quit_requested = 1; }
static void exit_clicked(lv_event_t *event) { (void)event; quit_requested = 1; }
static void draw_game(lv_event_t *event)
{
    if (!surface || !surface->pixels) return;
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(event);
    const lv_area_t *area = ctx->clip_area;
    int offset_x = (800 - surface->w) / 2, offset_y = (480 - surface->h) / 2;
    int stride = lv_area_get_width(ctx->buf_area);
    for (int y = area->y1; y <= area->y2; ++y) {
        lv_color_t *out = (lv_color_t *)ctx->buf + (y - ctx->buf_area->y1) * stride;
        for (int x = area->x1; x <= area->x2; ++x) {
            int sx = y - offset_x, sy = 479 - x - offset_y;
            lv_color_t color = lv_color_black();
            if (sx >= 0 && sx < surface->w && sy >= 0 && sy < surface->h) {
                const uint8_t *pixel = (const uint8_t *)surface->pixels + sy * surface->pitch + sx * surface->format->BytesPerPixel;
                uint32_t value = 0;
                memcpy(&value, pixel, surface->format->BytesPerPixel);
                uint8_t r, g, b; SDL_GetRGB(value, surface->format, &r, &g, &b);
                color = lv_color_make(r, g, b);
            }
            out[x - ctx->buf_area->x1] = color;
        }
    }
}
static void close_display(void)
{
    if (opened) { platform_close(); opened = 0; }
}
int SDL_Init(Uint32 flags)
{
    int (*real_init)(Uint32) = dlsym(RTLD_NEXT, "SDL_Init");
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    signal(SIGTERM, stop); signal(SIGINT, stop);
    return real_init(flags);
}
SDL_Surface *SDL_SetVideoMode(int width, int height, int depth, Uint32 flags)
{
    (void)flags;
    SDL_Surface *(*real_mode)(int, int, int, Uint32) = dlsym(RTLD_NEXT, "SDL_SetVideoMode");
    if (width < 1 || width > 800 || height < 1 || height > 480) {
        SDL_SetError("LVGL backend supports up to 800x480 game surfaces"); return NULL;
    }
    surface = real_mode(width, height, depth, SDL_SWSURFACE);
    if (!surface) return NULL;
    if (!opened) {
        if (platform_open()) { SDL_SetError("LVGL framebuffer initialization failed"); return NULL; }
        opened = 1; tick = start = platform_ms(); atexit(close_display);
        canvas = lv_obj_create(lv_scr_act()); lv_obj_remove_style_all(canvas);
        lv_obj_set_size(canvas, 480, 800); lv_obj_clear_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(canvas, draw_game, LV_EVENT_DRAW_MAIN, NULL);
        lv_obj_t *back = lv_btn_create(lv_scr_act());
        lv_obj_set_pos(back, 388, 708); lv_obj_set_size(back, 80, 80);
        lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(back, lv_color_hex(0x24343D), 0);
        lv_obj_set_style_radius(back, 8, 0);
        lv_obj_t *label = lv_label_create(back);
        lv_label_set_text(label, LV_SYMBOL_CLOSE); lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
        lv_obj_set_style_text_color(label, lv_color_white(), 0); lv_obj_center(label);
        lv_obj_add_event_cb(back, exit_clicked, LV_EVENT_CLICKED, NULL);
        fprintf(stderr, "ONS LVGL8.2 backend: game=%dx%d, native480x800 rotation90, SDL dummy only\n", width, height);
    }
    lv_obj_invalidate(canvas); lv_refr_now(NULL);
    return surface;
}
void SDL_UpdateRects(SDL_Surface *screen, int count, SDL_Rect *rects)
{
    (void)count; (void)rects;
    if (opened && screen == surface) { lv_obj_invalidate(canvas); lv_refr_now(NULL); }
}
void SDL_UpdateRect(SDL_Surface *screen, Sint32 x, Sint32 y, Uint32 w, Uint32 h)
{
    (void)x; (void)y; (void)w; (void)h; SDL_UpdateRects(screen, 0, NULL);
}
int SDL_Flip(SDL_Surface *screen) { SDL_UpdateRects(screen, 0, NULL); return 0; }
int SDL_PollEvent(SDL_Event *event)
{
    SDL_Event ignored;
    if (!event) event = &ignored;
    int (*real_poll)(SDL_Event *) = dlsym(RTLD_NEXT, "SDL_PollEvent");
    if (opened) {
        uint64_t now = platform_ms(); lv_tick_inc(now - tick); tick = now;
        lv_timer_handler();
        const char *limit = getenv("ONS_LVGL_TEST_MS");
        if (limit && now - start >= strtoul(limit, NULL, 10)) quit_requested = 1;
        if (quit_requested) { memset(event, 0, sizeof(*event)); event->type = SDL_QUIT; return 1; }
        int x, y, down; platform_touch(&x, &y, &down);
        int sx = y - (800 - surface->w) / 2, sy = 479 - x - (480 - surface->h) / 2;
        int exit_area = x >= 388 && y >= 708;
        if (down != old_down) {
            old_down = down;
            if ((!down && mouse_captured) || (!exit_area && sx >= 0 && sx < surface->w && sy >= 0 && sy < surface->h)) {
                if (!down) { sx = old_x; sy = old_y; }
                mouse_captured = down;
                memset(event, 0, sizeof(*event)); event->type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
                event->button.button = SDL_BUTTON_LEFT; event->button.state = down ? SDL_PRESSED : SDL_RELEASED;
                event->button.x = sx; event->button.y = sy; old_x = sx; old_y = sy; return 1;
            }
        } else if (down && mouse_captured && !exit_area && sx >= 0 && sx < surface->w && sy >= 0 && sy < surface->h && (sx != old_x || sy != old_y)) {
            memset(event, 0, sizeof(*event)); event->type = SDL_MOUSEMOTION;
            event->motion.state = SDL_BUTTON_LMASK; event->motion.x = sx; event->motion.y = sy;
            event->motion.xrel = sx - old_x; event->motion.yrel = sy - old_y; old_x = sx; old_y = sy; return 1;
        }
    }
    return real_poll(event);
}
int SDL_WaitEvent(SDL_Event *event)
{
    for (;;) {
        if (SDL_PollEvent(event)) return 1;
        struct timespec delay = {0, 10000000}; nanosleep(&delay, NULL);
    }
}
