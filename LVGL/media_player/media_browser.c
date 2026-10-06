#define _XOPEN_SOURCE 700
#include "platform.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern const lv_font_t menu_font_stream;
void menu_font_stream_close(void);
enum { MAX_ITEMS = 256, PAGE_ROWS = 5 };
struct entry { char name[256]; int directory; };
static struct entry items[MAX_ITEMS];
static lv_obj_t *row_buttons[PAGE_ROWS];
static char current[PATH_MAX], selected[PATH_MAX];
static unsigned count, page;
static int truncated, result;
static volatile sig_atomic_t quit;
static void stop(int signal) { (void)signal; quit = 1; }
static int join(char *out, size_t size, const char *directory, const char *name)
{
    int n = snprintf(out, size, "%s%s%s", directory, !strcmp(directory, "/") ? "" : "/", name);
    return n >= 0 && (size_t)n < size ? 0 : -1;
}
static int media_name(const char *name)
{
    const char *extension = strrchr(name, '.');
    return extension && (!strcasecmp(extension, ".mp3") || !strcasecmp(extension, ".mp4") ||
                         !strcasecmp(extension, ".wav"));
}
static int media_file(const char *path)
{
    struct stat info;
    return !strchr(path, '\n') && !strchr(path, '\r') && media_name(path) &&
        !stat(path, &info) && S_ISREG(info.st_mode) && !access(path, R_OK);
}
static int compare(const void *a, const void *b)
{
    const struct entry *left = a, *right = b;
    return left->directory != right->directory ? right->directory - left->directory : strcmp(left->name, right->name);
}
static int scan(const char *directory)
{
    char canonical[PATH_MAX];
    if (!realpath(directory, canonical) || strchr(canonical, '\n') || strchr(canonical, '\r')) return -1;
    DIR *dir = opendir(canonical); if (!dir) return -1;
    count = truncated = 0;
    struct dirent *item;
    while ((item = readdir(dir))) {
        if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, "..") ||
            strchr(item->d_name, '\n') || strchr(item->d_name, '\r')) continue;
        char path[PATH_MAX]; struct stat info;
        if (join(path, sizeof(path), canonical, item->d_name) || stat(path, &info)) continue;
        int directory_entry = S_ISDIR(info.st_mode);
        if (!directory_entry && !media_file(path)) continue;
        if (count == MAX_ITEMS) { truncated = 1; continue; }
        snprintf(items[count].name, sizeof(items[count].name), "%s", item->d_name);
        items[count++].directory = directory_entry;
    }
    closedir(dir); qsort(items, count, sizeof(*items), compare);
    strcpy(current, canonical); page = 0;
    return 0;
}
static lv_obj_t *text(lv_obj_t *parent, const char *value, int x, int y, int width)
{
    lv_obj_t *label = lv_label_create(parent); lv_obj_set_pos(label, x, y); lv_obj_set_width(label, width);
    lv_obj_set_style_text_font(label, &menu_font_stream, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x172329), 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT); lv_label_set_text(label, value);
    return label;
}
static lv_obj_t *button(int x, int y, int width, int height, const char *symbol)
{
    lv_obj_t *object = lv_btn_create(lv_scr_act()); lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, x, y); lv_obj_set_size(object, width, height);
    lv_obj_set_style_radius(object, 4, 0); lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(object, lv_color_white(), 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(0xD5DFE3), LV_STATE_PRESSED);
    if (symbol) {
        lv_obj_t *label = text(object, symbol, 0, 0, width);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0); lv_obj_center(label);
    }
    return object;
}
static void render(void);
static void back_clicked(lv_event_t *event) { (void)event; quit = 1; }
static void parent_clicked(lv_event_t *event)
{
    (void)event; char parent[PATH_MAX]; strcpy(parent, current);
    char *slash = strrchr(parent, '/'); if (!slash || slash == parent) strcpy(parent, "/"); else *slash = 0;
    if (!scan(parent)) render();
}
static void entry_clicked(lv_event_t *event)
{
    unsigned index = (unsigned)(uintptr_t)lv_event_get_user_data(event);
    if (index >= count) return;
    char path[PATH_MAX];
    if (join(path, sizeof(path), current, items[index].name)) return;
    if (items[index].directory) { if (!scan(path)) render(); return; }
    if (media_file(path)) { strcpy(selected, path); result = 20; quit = 1; }
}
static void page_clicked(lv_event_t *event)
{
    int next = (int)page + (int)(intptr_t)lv_event_get_user_data(event);
    if (next < 0 || next >= (int)((count + PAGE_ROWS - 1) / PAGE_ROWS)) return;
    page = next; render();
}
static void render(void)
{
    lv_obj_t *root = lv_scr_act(); lv_obj_clean(root);
    memset(row_buttons, 0, sizeof(row_buttons));
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root, lv_color_hex(0xF1F4F6), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_t *back = button(16, 16, 72, 72, LV_SYMBOL_LEFT);
    lv_obj_add_event_cb(back, back_clicked, LV_EVENT_CLICKED, NULL);
    text(root, "本地播放器", 110, 34, 270);
    lv_obj_t *up = button(392, 16, 72, 72, LV_SYMBOL_UP);
    lv_obj_add_event_cb(up, parent_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *path = text(root, current, 20, 104, 440); lv_obj_set_height(path, 64);
    lv_obj_set_style_text_font(path, &menu_font_stream, 0);
    for (unsigned row = 0; row < PAGE_ROWS && page * PAGE_ROWS + row < count; ++row) {
        unsigned i = page * PAGE_ROWS + row;
        lv_obj_t *entry = button(16, 180 + row * 84, 448, 76, NULL);
        row_buttons[row] = entry;
        lv_obj_t *icon = text(entry, items[i].directory ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE, 16, 23, 44);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
        lv_obj_t *name = text(entry, items[i].name, 76, 20, 352); lv_obj_set_height(name, 40);
        lv_obj_add_event_cb(entry, entry_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }
    if (!count) text(root, "空目录", 32, 210, 416);
    unsigned pages = (count + PAGE_ROWS - 1) / PAGE_ROWS; if (!pages) pages = 1;
    char page_text[40]; snprintf(page_text, sizeof(page_text), "%u / %u%s", page + 1, pages, truncated ? " +" : "");
    for (unsigned side = 0; side < 2; ++side) {
        lv_obj_t *change = button(side ? 392 : 16, 624, 72, 64, side ? LV_SYMBOL_RIGHT : LV_SYMBOL_LEFT);
        lv_obj_add_event_cb(change, page_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)(side ? 1 : -1));
        if ((!side && !page) || (side && page + 1 == pages)) lv_obj_add_state(change, LV_STATE_DISABLED);
    }
    lv_obj_t *counter = text(root, page_text, 120, 642, 240);
    lv_obj_set_style_text_align(counter, LV_TEXT_ALIGN_CENTER, 0);
}
int main(int argc, char **argv)
{
    const char *directory = argc > 1 ? argv[1] : "/root/roms";
    if (argc > 2 && !strcmp(argv[2], "--check")) return media_file(directory) ? 0 : 1;
    if (scan(directory)) { fprintf(stderr, "Media directory: %s: %s\n", directory, strerror(errno)); return 1; }
    if (argc > 2 && !strcmp(argv[2], "--list")) {
        for (unsigned i = 0; i < count; ++i) printf("%c %s\n", items[i].directory ? 'D' : 'S', items[i].name);
        return truncated ? 1 : 0;
    }
    if (platform_open()) { platform_close(); return 2; }
    signal(SIGTERM, stop); signal(SIGINT, stop);
    render(); uint64_t tick = platform_ms(), start = tick;
    const char *limit = getenv("MEDIA_BROWSER_TEST_MS"); unsigned long duration = limit ? strtoul(limit, NULL, 10) : 0;
    const char *test_file = getenv("MEDIA_BROWSER_TEST_SELECT");
    while (!quit) {
        uint64_t now = platform_ms(); lv_tick_inc(now - tick); tick = now;
        unsigned delay = lv_timer_handler(); if (delay < 1) delay = 1; if (delay > 16) delay = 16;
        if (test_file && now - start >= 250) {
            unsigned index;
            for (index = 0; index < count; ++index)
                if (!items[index].directory && !strcmp(items[index].name, test_file)) break;
            if (index == count) { result = 1; quit = 1; }
            else {
                page = index / PAGE_ROWS; render();
                lv_event_send(row_buttons[index % PAGE_ROWS], LV_EVENT_CLICKED, NULL);
            }
            test_file = NULL;
        }
        if (duration && now - start >= duration) quit = 1;
        usleep(delay * 1000);
    }
    platform_close(); menu_font_stream_close();
    if (result == 20) printf("%s\n", selected);
    return result;
}
