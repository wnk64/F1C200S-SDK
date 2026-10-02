#define _POSIX_C_SOURCE 200809L
#include "platform.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

LV_FONT_DECLARE(menu_font_28);
enum { MAX_APPS = 12, PER_PAGE = 3 };
struct app { char id[32], title[80], subtitle[128], icon[16], command[256]; };
static struct app apps[MAX_APPS];
static unsigned count, page;
static volatile sig_atomic_t quit;
static int exit_code, view;
static lv_obj_t *content, *status_label, *touch_label, *marker, *brightness_label;
static const char *config_path;
static void signal_stop(int n) { (void)n; quit = 1; }
static lv_obj_t *label(lv_obj_t *parent, const char *text, int px, int py, int width, uint32_t color)
{
    lv_obj_t *o = lv_label_create(parent);
    lv_obj_set_width(o, width); lv_label_set_long_mode(o, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(o, &menu_font_28, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_label_set_text(o, text); lv_obj_set_pos(o, px, py);
    return o;
}
static lv_obj_t *button(lv_obj_t *parent, int x, int y, int width, int height, uint32_t color)
{
    lv_obj_t *b = lv_btn_create(parent); lv_obj_remove_style_all(b);
    lv_obj_set_pos(b, x, y); lv_obj_set_size(b, width, height);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0xD5DFE3), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(0xDDE3E7), 0);
    return b;
}
static void home(void);
static void settings(void);
static void touch_test(void);
static void back(lv_event_t *e) { (void)e; home(); }
static void header(const char *title, int can_back)
{
    view = 0; touch_label = marker = brightness_label = NULL;
    lv_obj_clean(content);
    label(content, title, can_back ? 104 : 28, 34, can_back ? 320 : 424, 0x172329);
    if (can_back) {
        lv_obj_t *b = button(content, 20, 20, 72, 72, 0xFFFFFF);
        lv_obj_t *s = label(b, LV_SYMBOL_LEFT, 0, 0, 60, 0x172329); lv_obj_center(s);
        lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_add_event_cb(b, back, LV_EVENT_CLICKED, NULL);
    }
    status_label = label(content, "", 28, 105, 424, 0x66757E);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
}
static void launch(lv_event_t *e)
{
    unsigned index = (unsigned)(uintptr_t)lv_event_get_user_data(e);
    if (index >= count || quit) return;
    if (!strcmp(apps[index].command, "@touch")) { touch_test(); return; }
    if (!strcmp(apps[index].command, "@settings")) { settings(); return; }
    if (apps[index].command[0] != '/') return;
    exit_code = 20 + index; quit = 1;
}
static void change_page(lv_event_t *e)
{
    int direction = (int)(intptr_t)lv_event_get_user_data(e);
    int next = (int)page + direction;
    if (next < 0 || next >= (int)((count + PER_PAGE - 1) / PER_PAGE)) return;
    page = (unsigned)next; home();
}
static void home(void)
{
    header("应用", 0);
    label(content, "F1C200S", 312, 39, 144, 0x66757E);
    for (unsigned row = 0; row < PER_PAGE && page * PER_PAGE + row < count; row++) {
        unsigned i = page * PER_PAGE + row;
        uint32_t accent = i == 0 ? 0x167A6F : i == 1 ? 0x3473A1 : 0x6C687D;
        lv_obj_t *b = button(content, 24, 158 + row * 176, 432, 156, 0xFFFFFF);
        lv_obj_t *symbol = label(b, apps[i].icon, 24, 46, 62, accent);
        lv_obj_set_style_text_font(symbol, &lv_font_montserrat_32, 0);
        label(b, apps[i].title, 104, 30, 292, 0x172329);
        lv_obj_t *sub = label(b, apps[i].subtitle, 104, 84, 304, 0x66757E);
        lv_obj_set_style_text_font(sub, &menu_font_28, 0);
        lv_obj_add_event_cb(b, launch, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }
    unsigned pages = (count + PER_PAGE - 1) / PER_PAGE;
    if (pages > 1) {
        for (int side = 0; side < 2; side++) {
            lv_obj_t *b = button(content, side ? 368 : 24, 704, 88, 72, 0xFFFFFF);
            lv_obj_t *s = label(b, side ? LV_SYMBOL_RIGHT : LV_SYMBOL_LEFT, 0, 0, 60, 0x172329);
            lv_obj_center(s); lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_add_event_cb(b, change_page, LV_EVENT_CLICKED, (void *)(intptr_t)(side ? 1 : -1));
            if ((!side && !page) || (side && page + 1 == pages)) lv_obj_add_state(b, LV_STATE_DISABLED);
        }
        lv_obj_t *p = label(content, "", 168, 728, 144, 0x66757E);
        lv_label_set_text_fmt(p, "%u / %u", page + 1, pages);
        lv_obj_set_style_text_align(p, LV_TEXT_ALIGN_CENTER, 0);
    }
    if (getenv("F1_MENU_ERROR")) label(content, "启动失败，请检查日志", 28, 700, 424, 0xB84545);
}
static void brightness(lv_event_t *e)
{
    int current = platform_brightness(-1);
    int next = current + (int)(intptr_t)lv_event_get_user_data(e);
    /* Keep a visible recovery button; never allow accidental black screen. */
    if (current < 0 || next < 1 || next > 8) return;
    if (platform_brightness(next) >= 0) lv_label_set_text_fmt(brightness_label, "%d / 8", next);
}
static void settings(void)
{
    header("系统设置", 1); view = 1;
    label(content, "显示亮度", 28, 178, 424, 0x172329);
    brightness_label = label(content, "", 150, 276, 180, 0x172329);
    lv_obj_set_style_text_align(brightness_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text_fmt(brightness_label, "%d / 8", platform_brightness(-1));
    for (int side = 0; side < 2; side++) {
        lv_obj_t *b = button(content, side ? 350 : 24, 252, 106, 104, 0xFFFFFF);
        lv_obj_t *s = label(b, side ? LV_SYMBOL_PLUS : LV_SYMBOL_MINUS, 0, 0, 60, 0x167A6F);
        lv_obj_center(s); lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_add_event_cb(b, brightness, LV_EVENT_CLICKED, (void *)(intptr_t)(side ? 1 : -1));
    }
    label(content, "LVGL 8.2.0\n480 x 800 / 60 Hz", 28, 448, 424, 0x66757E);
}
static void test_hit(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target(e);
    lv_obj_set_style_bg_color(b, lv_color_hex(0xCBE8DE), 0);
    lv_label_set_text(lv_obj_get_child(b, 0), LV_SYMBOL_OK);
}
static void touch_test(void)
{
    header("触摸测试", 1); view = 2;
    for (unsigned i = 0; i < 4; i++) {
        lv_obj_t *b = button(content, (i & 1) ? 324 : 24, (i & 2) ? 632 : 172, 132, 132, 0xFFFFFF);
        lv_obj_t *l = label(b, (const char *[]) {"01", "02", "03", "04"}[i], 0, 0, 80, 0x3473A1);
        lv_obj_center(l); lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_add_event_cb(b, test_hit, LV_EVENT_CLICKED, NULL);
    }
    touch_label = label(content, "X 000   Y 000", 64, 408, 352, 0x172329);
    lv_obj_set_style_text_align(touch_label, LV_TEXT_ALIGN_CENTER, 0);
    marker = lv_obj_create(content); lv_obj_remove_style_all(marker);
    lv_obj_set_size(marker, 14, 14); lv_obj_set_style_radius(marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(marker, lv_color_hex(0xD74747), 0);
    lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, 0);
    lv_obj_clear_flag(marker, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_flag(marker, LV_OBJ_FLAG_HIDDEN);
}
static int read_apps(void)
{
    FILE *f = fopen(config_path, "r"); if (!f) return -1;
    char line[600];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        if (count == MAX_APPS) { fclose(f); return -1; }
        line[strcspn(line, "\r\n")] = 0;
        char *fields[5], *save = NULL;
        for (int i = 0; i < 5; i++) fields[i] = strtok_r(i ? NULL : line, "|", &save);
        if (!fields[4] || strtok_r(NULL, "|", &save)) { fclose(f); return -1; }
        struct app *a = &apps[count];
        if (strlen(fields[0]) >= sizeof(a->id) || strlen(fields[1]) >= sizeof(a->title) ||
            strlen(fields[2]) >= sizeof(a->subtitle) || strlen(fields[4]) >= sizeof(a->command)) { fclose(f); return -1; }
        if (fields[4][0] != '/' && strcmp(fields[4], "@touch") && strcmp(fields[4], "@settings")) { fclose(f); return -1; }
        snprintf(a->id, sizeof(a->id), "%s", fields[0]);
        snprintf(a->title, sizeof(a->title), "%s", fields[1]);
        snprintf(a->subtitle, sizeof(a->subtitle), "%s", fields[2]);
        snprintf(a->command, sizeof(a->command), "%s", fields[4]);
        const char *icon = !strcmp(fields[3], "cast") ? LV_SYMBOL_VIDEO :
            !strcmp(fields[3], "touch") ? LV_SYMBOL_EDIT : LV_SYMBOL_SETTINGS;
        snprintf(a->icon, sizeof(a->icon), "%s", icon); count++;
    }
    fclose(f); return count ? 0 : -1;
}
int main(int argc, char **argv)
{
    config_path = argc > 1 ? argv[1] : "apps.conf";
    if (read_apps()) { fprintf(stderr, "menu: invalid apps config\n"); return 1; }
    if (argc > 2 && !strcmp(argv[2], "--validate")) return 0;
    int smoke = argc > 2 && !strcmp(argv[2], "--smoke");
    if (platform_open()) { platform_close(); return 2; }
    signal(SIGTERM, signal_stop); signal(SIGINT, signal_stop);
    content = lv_scr_act(); lv_obj_set_style_bg_color(content, lv_color_hex(0xF1F4F6), 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    home();
    uint64_t previous = platform_ms(), start = previous, update = 0;
    int smoke_stage = 0;
    while (!quit) {
        uint64_t now = platform_ms(); lv_tick_inc((uint32_t)(now - previous)); previous = now;
        lv_timer_handler();
        if (now - update >= 250) {
            update = now;
            lv_label_set_text(status_label, access("/sys/class/net/rtlwlan0", F_OK) ? "Wi-Fi --" : "Wi-Fi  /  F1C200S");
            if (view == 2) {
                int x, y, down; platform_touch(&x, &y, &down);
                lv_label_set_text_fmt(touch_label, "X %03d   Y %03d", x, y);
                if (down) { lv_obj_clear_flag(marker, LV_OBJ_FLAG_HIDDEN); lv_obj_set_pos(marker, x - 7, y - 7); }
                else lv_obj_add_flag(marker, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (smoke && now - start > (uint64_t)(smoke_stage + 1) * 1000) {
            if (smoke_stage == 0) settings();
            else if (smoke_stage == 1) touch_test();
            else if (smoke_stage == 2) home();
            else quit = 1;
            smoke_stage++;
        }
        struct timespec delay = {0, 5000000}; nanosleep(&delay, NULL);
    }
    fprintf(stderr, "menu: exit=%d heap=%u\n", exit_code, (unsigned)LV_MEM_SIZE);
    if (exit_code >= 20 && exit_code < 20 + (int)count) {
        lv_obj_clean(content);
        label(content, apps[exit_code - 20].title, 64, 330, 352, 0x167A6F);
        lv_refr_now(NULL);
    }
    platform_close(); return exit_code;
}
