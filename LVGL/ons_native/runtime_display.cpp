#include "engine_runtime.h"
extern "C" {
#include "platform.h"
}
#include <pthread.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <atomic>
#include <csignal>

struct ons_rt_timer {
    bool active, running;
    uint64_t due;
    uint32_t interval;
    uint32_t (*callback)(uint32_t, void *);
    void *argument;
};
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static pthread_t timer_thread;
static bool initialized, timer_started, display_opened;
static std::atomic<bool> ending(false);
static ons_rt_timer timers[32];
static ons_rt_Event pending[128];
static unsigned queued;
static ons_surface *screen;
static lv_obj_t *canvas;
static uint64_t tick, origin;
static int mouse_x, mouse_y, mouse_down, finger_down, captured;
static int cursor_visible;
static char caption[256];
static thread_local char error[128];
static volatile sig_atomic_t quit_signal;
static uint64_t test_duration;
static bool test_requested;
static void request_quit(int) { quit_signal = 1; }
static void failure(const char *message) { std::snprintf(error, sizeof(error), "%s", message); }
const char *ons_rt_GetError(void) { return error; }
static uint64_t milliseconds()
{
    timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}
uint32_t ons_rt_GetTicks(void) { return (uint32_t)(milliseconds() - origin); }
int ons_rt_PushEvent(ons_rt_Event *event)
{
    if (!event || event->type >= 32) return -1;
    if (std::getenv("ONS_NATIVE_INPUT_DEBUG") &&
        (event->type == ons_rt_MOUSEBUTTONDOWN || event->type == ons_rt_MOUSEBUTTONUP || event->type == ons_rt_QUIT))
        std::fprintf(stderr, "Native input type=%u x=%d y=%d\n", event->type, event->button.x, event->button.y);
    pthread_mutex_lock(&lock);
    if (ending || queued == 128) { pthread_mutex_unlock(&lock); failure("Native event queue full or closed"); return -1; }
    if (event->type == ons_rt_MOUSEMOTION && queued && pending[queued - 1].type == ons_rt_MOUSEMOTION)
        pending[queued - 1] = *event;
    else pending[queued++] = *event;
    pthread_cond_broadcast(&changed); pthread_mutex_unlock(&lock); return 0;
}
int ons_rt_PeepEvents(ons_rt_Event *events, int count, int action, uint32_t mask)
{
    if (count < 0 || (count && !events) || (action != ons_rt_PEEKEVENT && action != ons_rt_GETEVENT)) return -1;
    pthread_mutex_lock(&lock); int copied = 0;
    for (unsigned i = 0; i < queued && copied < count;) {
        if (!(mask & ons_rt_EVENTMASK(pending[i].type))) { ++i; continue; }
        events[copied++] = pending[i];
        if (action == ons_rt_GETEVENT) {
            --queued; std::memmove(pending + i, pending + i + 1, (queued - i) * sizeof(*pending));
        } else ++i;
    }
    pthread_mutex_unlock(&lock); return copied;
}
static void *timer_loop(void *)
{
    pthread_mutex_lock(&lock);
    while (!ending) {
        uint64_t now = milliseconds(); int selected = -1;
        for (unsigned i = 0; i < 32; ++i)
            if (timers[i].active && !timers[i].running && timers[i].due <= now &&
                (selected < 0 || timers[i].due < timers[selected].due)) selected = i;
        if (selected >= 0) {
            ons_rt_timer *timer = timers + selected;
            timer->running = true;
            auto callback = timer->callback; void *argument = timer->argument; uint32_t interval = timer->interval;
            pthread_mutex_unlock(&lock);
            uint32_t next = callback(interval, argument);
            pthread_mutex_lock(&lock); timer->running = false;
            if (timer->active) { timer->active = next != 0; timer->interval = next; timer->due = milliseconds() + next; }
            pthread_cond_broadcast(&changed); continue;
        }
        /* A short bounded wait avoids wall-clock adjustments affecting timers. */
        pthread_mutex_unlock(&lock); timespec pause = {0, 1000000}; nanosleep(&pause, NULL); pthread_mutex_lock(&lock);
    }
    pthread_mutex_unlock(&lock); return NULL;
}
ons_rt_TimerID ons_rt_AddTimer(uint32_t interval, uint32_t (*callback)(uint32_t, void *), void *argument)
{
    if (!initialized || !interval || !callback) return NULL;
    pthread_mutex_lock(&lock);
    for (unsigned i = 0; i < 32; ++i) if (!timers[i].active && !timers[i].running) {
        timers[i].active = true; timers[i].due = milliseconds() + interval;
        timers[i].interval = interval; timers[i].callback = callback; timers[i].argument = argument;
        pthread_mutex_unlock(&lock); return timers + i;
    }
    pthread_mutex_unlock(&lock); failure("Native timer capacity exhausted"); return NULL;
}
int ons_rt_RemoveTimer(ons_rt_TimerID timer)
{
    if (!timer) return 0;
    pthread_mutex_lock(&lock);
    ons_rt_timer *found = NULL;
    for (unsigned i = 0; i < 32; ++i) if (timers + i == timer) found = timers + i;
    if (!found) { pthread_mutex_unlock(&lock); return 0; }
    bool active = found->active; found->active = false;
    while (found->running && !pthread_equal(pthread_self(), timer_thread)) pthread_cond_wait(&changed, &lock);
    pthread_mutex_unlock(&lock); return active ? 1 : 0;
}
void ons_rt_Quit(void)
{
    if (!initialized) return;
    pthread_mutex_lock(&lock); ending = true; pthread_cond_broadcast(&changed); pthread_mutex_unlock(&lock);
    if (timer_started) { pthread_join(timer_thread, NULL); timer_started = false; }
    if (display_opened) { platform_close(); display_opened = false; canvas = NULL; }
    ons_surface_free(screen); screen = NULL;
    initialized = false;
}
int ons_rt_Init(uint32_t flags)
{
    if (flags & (ons_rt_INIT_CDROM | ons_rt_INIT_JOYSTICK)) { failure("CD/joystick hardware unavailable"); return -1; }
    if (initialized) return 0;
    origin = milliseconds(); ending = false; queued = 0; std::memset(timers, 0, sizeof(timers));
    const char *limit = std::getenv("ONS_NATIVE_TEST_MS"); test_duration = limit ? std::strtoul(limit, NULL, 10) : 0;
    quit_signal = 0; test_requested = false;
    std::signal(SIGTERM, request_quit); std::signal(SIGINT, request_quit);
    if (pthread_create(&timer_thread, NULL, timer_loop, NULL)) { failure("Native timer thread creation failed"); return -1; }
    timer_started = initialized = true; std::atexit(ons_rt_Quit); return 0;
}
int ons_rt_InitSubSystem(uint32_t flags)
{
    if (flags & (ons_rt_INIT_CDROM | ons_rt_INIT_JOYSTICK)) { failure("CD/joystick hardware unavailable"); return -1; }
    return initialized ? 0 : ons_rt_Init(flags);
}
static void draw(lv_event_t *event)
{
    if (!screen) return;
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(event);
    const lv_area_t *area = ctx->clip_area; int stride = lv_area_get_width(ctx->buf_area);
    int ox = (800 - screen->w) / 2, oy = (480 - screen->h) / 2;
    for (int y = area->y1; y <= area->y2; ++y) {
        lv_color_t *output = (lv_color_t *)ctx->buf + (y - ctx->buf_area->y1) * stride;
        for (int x = area->x1; x <= area->x2; ++x) {
            int sx = y - ox, sy = 479 - x - oy;
            uint32_t pixel = sx >= 0 && sx < screen->w && sy >= 0 && sy < screen->h ?
                screen->pixels[sy * screen->w + sx] : 0;
            output[x - ctx->buf_area->x1].full = ((pixel >> 8) & 0xf800) | ((pixel >> 5) & 0x7e0) | ((pixel >> 3) & 31);
        }
    }
}
static void exit_clicked(lv_event_t *) { ons_rt_Event event = {}; event.type = ons_rt_QUIT; ons_rt_PushEvent(&event); }
ons_surface *ons_rt_SetVideoMode(int w, int h, int depth, uint32_t)
{
    if (!initialized || depth != 32 || w < 1 || w > 800 || h < 1 || h > 480) { failure("Native viewport must fit 800x480 in ARGB8888"); return NULL; }
    if (screen) { failure("Native viewport already exists"); return NULL; }
    screen = ons_surface_new(w, h); if (!screen) return NULL;
    if (platform_open()) { platform_close(); ons_surface_free(screen); screen = NULL; failure("LVGL framebuffer setup failed"); return NULL; }
    display_opened = true; tick = milliseconds();
    canvas = lv_obj_create(lv_scr_act()); lv_obj_remove_style_all(canvas); lv_obj_set_size(canvas, 480, 800);
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(canvas, draw, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_t *button = lv_btn_create(lv_scr_act()); lv_obj_set_pos(button, 388, 720); lv_obj_set_size(button, 80, 80);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(button, lv_color_hex(0x24343d), 0);
    lv_obj_t *label = lv_label_create(button); lv_label_set_text(label, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0); lv_obj_set_style_text_color(label, lv_color_white(), 0); lv_obj_center(label);
    lv_obj_add_event_cb(button, exit_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_invalidate(canvas); lv_refr_now(NULL); return screen;
}
ons_rect **ons_rt_ListModes(ons_pixel_format *, uint32_t)
{
    static ons_rect native = {0, 0, 800, 480}; static ons_rect *modes[] = {&native, NULL}; return modes;
}
void ons_rt_UpdateRect(ons_surface *surface, int x, int y, uint32_t width, uint32_t height)
{
    if (!display_opened || surface != screen) return;
    if (!width || !height) lv_obj_invalidate(canvas);
    else {
        int64_t x1 = std::max<int64_t>(0, x), y1 = std::max<int64_t>(0, y);
        int64_t x2 = std::min<int64_t>(screen->w, (int64_t)x + width), y2 = std::min<int64_t>(screen->h, (int64_t)y + height);
        if (x2 <= x1 || y2 <= y1) return;
        int ox = (800 - screen->w) / 2, oy = (480 - screen->h) / 2;
        lv_area_t area = {(int16_t)(480 - y2 - oy), (int16_t)(x1 + ox), (int16_t)(479 - y1 - oy), (int16_t)(x2 - 1 + ox)};
        lv_obj_invalidate_area(canvas, &area);
    }
    lv_refr_now(NULL);
}
static void pump()
{
    if (!display_opened) return;
    uint64_t now = milliseconds(); lv_tick_inc(now - tick); tick = now; lv_timer_handler();
    if (quit_signal || (!test_requested && test_duration && now - origin >= test_duration)) {
        ons_rt_Event event = {}; event.type = ons_rt_QUIT;
        if (!ons_rt_PushEvent(&event)) { quit_signal = 0; test_requested = true; }
    }
    int x, y, down; platform_touch(&x, &y, &down);
    int sx = y - (800 - screen->w) / 2, sy = 479 - x - (480 - screen->h) / 2;
    bool inside = sx >= 0 && sx < screen->w && sy >= 0 && sy < screen->h && !(x >= 388 && y >= 720);
    bool edge = down != finger_down; finger_down = down;
    if (!captured && !(edge && down && inside)) return;
    if (!inside) { sx = screen->w; sy = screen->h; }
    if (!edge && sx == mouse_x && sy == mouse_y) return;
    ons_rt_Event event = {}; event.type = ons_rt_MOUSEMOTION;
    event.motion.x = sx; event.motion.y = sy; event.motion.xrel = sx - mouse_x; event.motion.yrel = sy - mouse_y;
    event.motion.state = mouse_down ? ons_rt_BUTTON(ons_rt_BUTTON_LEFT) : 0;
    mouse_x = sx; mouse_y = sy; ons_rt_PushEvent(&event);
    if (edge) {
        captured = mouse_down = down; event = {};
        event.type = down ? ons_rt_MOUSEBUTTONDOWN : ons_rt_MOUSEBUTTONUP;
        event.button.x = sx; event.button.y = sy; event.button.button = ons_rt_BUTTON_LEFT; event.button.state = down;
        ons_rt_PushEvent(&event);
    }
}
int ons_rt_PollEvent(ons_rt_Event *event) { pump(); ons_rt_Event ignored; return ons_rt_PeepEvents(event ? event : &ignored, 1, ons_rt_GETEVENT, ons_rt_ALLEVENTS); }
int ons_rt_WaitEvent(ons_rt_Event *event)
{
    while (!ending) { int result = ons_rt_PollEvent(event); if (result) return result; timespec pause = {0, 5000000}; nanosleep(&pause, NULL); }
    return 0;
}
void ons_rt_Delay(uint32_t duration)
{
    uint64_t end = milliseconds() + duration;
    while (milliseconds() < end && !ending) { pump(); timespec pause = {0, 1000000}; nanosleep(&pause, NULL); }
}
uint8_t ons_rt_GetMouseState(int *x, int *y) { if (x) *x = mouse_x; if (y) *y = mouse_y; return mouse_down ? ons_rt_BUTTON(ons_rt_BUTTON_LEFT) : 0; }
uint8_t ons_rt_GetAppState(void) { return display_opened ? ons_rt_APPACTIVE | ons_rt_APPMOUSEFOCUS : 0; }
void ons_rt_WarpMouse(int x, int y)
{
    ons_rt_Event event = {}; event.type = ons_rt_MOUSEMOTION; event.motion.x = x; event.motion.y = y;
    event.motion.xrel = x - mouse_x; event.motion.yrel = y - mouse_y; mouse_x = x; mouse_y = y; ons_rt_PushEvent(&event);
}
int ons_rt_ShowCursor(int visible) { int previous = cursor_visible; if (visible >= 0) cursor_visible = visible; return previous; }
void ons_rt_WM_SetCaption(const char *title, const char *) { std::snprintf(caption, sizeof(caption), "%s", title ? title : ""); }
int ons_rt_WM_ToggleFullScreen(ons_surface *surface) { return surface == screen && display_opened ? 1 : 0; }
void *ons_rt_JoystickOpen(int) { failure("Joystick unavailable on touch-only target"); return NULL; }
