#define _POSIX_C_SOURCE 200809L
#include "time-settings.h"
#include "ui.h"
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static lv_obj_t *fields[5], *clock_label, *save_button, *sync_button;
static pid_t child;
static uint64_t started, last_update;
static int active, last_result;
static const char *tool = "/opt/f1-time/f1-rtc-time-ui";
LV_FONT_DECLARE(menu_font_28);

void time_settings_format_clock(char *text, size_t length)
{
    static time_t floor;
    if (!floor) {
        FILE *status = fopen("/run/f1-rtc-time.status", "r");
        if (status) {
            char line[160]; long value;
            while (fgets(line, sizeof(line), status)) {
                if (sscanf(line, "kernel_build_utc=%ld", &value) == 1 && value > 0) {
                    floor = (time_t)value; break;
                }
            }
            fclose(status);
        }
    }
    time_t t = time(NULL);
    if (!floor || t < floor) { snprintf(text, length, "Syncing time..."); return; }
    t += 8 * 3600;
    struct tm date; gmtime_r(&t, &date);
    strftime(text, length, "%Y-%m-%d  %H:%M:%S", &date);
}

void time_settings_close(void)
{
    active = 0;
    if (child > 0) {
        kill(child, SIGTERM);
        while (waitpid(child, NULL, 0) < 0 && errno == EINTR) {}
        child = 0;
    }
}
static void finish_view(void) { time_settings_close(); menu_settings(); }
static void enable_buttons(int enabled)
{
    for (int i = 0; i < 5; ++i) {
        if (enabled) lv_obj_clear_state(fields[i], LV_STATE_DISABLED);
        else lv_obj_add_state(fields[i], LV_STATE_DISABLED);
    }
    if (enabled) {
        lv_obj_clear_state(save_button, LV_STATE_DISABLED);
        lv_obj_clear_state(sync_button, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(save_button, LV_STATE_DISABLED);
        lv_obj_add_state(sync_button, LV_STATE_DISABLED);
    }
}
static void operate(lv_event_t *event)
{
    if (child > 0) return;
    int network = (int)(intptr_t)lv_event_get_user_data(event);
    char date[40];
    snprintf(date, sizeof(date), "%04u-%02u-%02uT%02u:%02u:00",
        2026U + lv_roller_get_selected(fields[0]), 1U + lv_roller_get_selected(fields[1]),
        1U + lv_roller_get_selected(fields[2]), lv_roller_get_selected(fields[3]),
        lv_roller_get_selected(fields[4]));
    child = fork();
    if (!child) {
        int log = open("/run/f1-menu-time.status", O_CREAT | O_TRUNC | O_WRONLY, 0600);
        if (log >= 0) { dup2(log, STDOUT_FILENO); dup2(log, STDERR_FILENO); close(log); }
        if (network) execl(tool, tool, "sync", (char *)NULL);
        else execl(tool, tool, "set-local", date, (char *)NULL);
        _exit(127);
    }
    if (child < 0) { child = 0; lv_label_set_text(menu_status(), "无法启动校时"); return; }
    started = platform_ms();
    last_result = -1;
    enable_buttons(0);
    lv_label_set_text(menu_status(), network ? "正在联网校时…" : "正在保存时间…");
}
static lv_obj_t *roller(int x, int y, int width, int start, int count, int selected)
{
    char options[400]; size_t length = 0;
    for (int i = 0; i < count; ++i)
        length += snprintf(options + length, sizeof(options) - length,
                           i ? "\n%02d" : "%02d", start + i);
    lv_obj_t *r = lv_roller_create(menu_content());
    lv_roller_set_options(r, options, LV_ROLLER_MODE_NORMAL);
    lv_obj_set_style_text_font(r, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_font(r, &lv_font_montserrat_24, LV_PART_SELECTED);
    lv_obj_set_style_text_line_space(r, 12, 0);
    lv_obj_set_style_text_line_space(r, 12, LV_PART_SELECTED);
    lv_obj_set_style_text_color(r, lv_color_hex(0x66757E), 0);
    lv_obj_set_style_bg_color(r, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_border_color(r, lv_color_hex(0xDDE3E7), 0);
    lv_obj_set_style_bg_color(r, lv_color_hex(0x167A6F), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_text_color(r, lv_color_hex(0xFFFFFF), LV_PART_SELECTED);
    lv_roller_set_visible_row_count(r, 3);
    lv_obj_set_pos(r, x, y); lv_obj_set_size(r, width, 124);
    lv_obj_set_style_radius(r, 8, 0);
    lv_roller_set_selected(r, selected, LV_ANIM_OFF);
    return r;
}
static void seed_fields(void)
{
    time_t t = time(NULL) + 8 * 3600;
    struct tm date; gmtime_r(&t, &date);
    if (date.tm_year < 126 || date.tm_year > 137) {
        date = (struct tm){.tm_year = 126, .tm_mon = 9, .tm_mday = 3, .tm_hour = 12};
    }
    int values[] = {date.tm_year - 126, date.tm_mon, date.tm_mday - 1, date.tm_hour, date.tm_min};
    for (int i = 0; i < 5; ++i) lv_roller_set_selected(fields[i], values[i], LV_ANIM_OFF);
}
void time_settings_show(void)
{
    header("日期与时间", 1); menu_back_to(finish_view); active = 1;
    lv_obj_set_style_text_font(menu_status(), &menu_font_28, 0);
    lv_label_set_text(menu_status(), "北京时间 UTC+8");
    clock_label = label(menu_content(), "", 28, 160, 424, 0x172329);
    lv_obj_set_style_text_font(clock_label, &lv_font_montserrat_24, 0);
    const char *names[] = {"年", "月", "日", "时", "分"};
    for (int i = 0; i < 3; ++i) label(menu_content(), names[i], 28 + i * 144, 210, 132, 0x66757E);
    fields[0] = roller(24, 252, 136, 2026, 12, 0);
    fields[1] = roller(172, 252, 136, 1, 12, 0);
    fields[2] = roller(320, 252, 136, 1, 31, 0);
    label(menu_content(), names[3], 28, 400, 200, 0x66757E);
    label(menu_content(), names[4], 256, 400, 200, 0x66757E);
    fields[3] = roller(24, 442, 204, 0, 24, 0);
    fields[4] = roller(252, 442, 204, 0, 60, 0);
    seed_fields();
    save_button = button(menu_content(), 24, 600, 432, 78, 0xFFFFFF);
    sync_button = button(menu_content(), 24, 696, 432, 78, 0xFFFFFF);
    label(save_button, LV_SYMBOL_SAVE, 24, 22, 48, 0x167A6F);
    label(save_button, "保存时间", 92, 22, 300, 0x172329);
    label(sync_button, LV_SYMBOL_REFRESH, 24, 22, 48, 0x167A6F);
    label(sync_button, "联网校时", 92, 22, 300, 0x172329);
    lv_obj_add_event_cb(save_button, operate, LV_EVENT_CLICKED, (void *)0);
    lv_obj_add_event_cb(sync_button, operate, LV_EVENT_CLICKED, (void *)1);
    last_update = 0;
    time_settings_poll();
}
void time_settings_poll(void)
{
    if (!active) return;
    uint64_t now = platform_ms();
    if (now - last_update >= 1000 || !last_update) {
        char text[40]; time_settings_format_clock(text, sizeof(text));
        lv_label_set_text(clock_label, text); last_update = now;
    }
    if (child > 0) {
        int status;
        pid_t result = waitpid(child, &status, WNOHANG);
        if (result == child) {
            child = 0; enable_buttons(1);
            int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            last_result = rc;
            const char *message = rc == 0 ? "时间已保存至 RTC" :
                rc == 3 ? "时间已更新，RTC 写入失败" :
                rc == 4 ? "校时忙，请稍后重试" :
                rc == 5 ? "日期无效或早于编译时间" : "校时失败，请检查网络";
            lv_label_set_text(menu_status(), message);
            if (!rc) seed_fields();
            last_update = 0;
        } else if (now - started > 25000) {
            kill(child, SIGTERM); waitpid(child, NULL, 0); child = 0;
            enable_buttons(1); lv_label_set_text(menu_status(), "校时超时，请检查网络");
            last_result = -1;
        }
    }
}
int time_settings_test_step(void)
{
    static int stage;
    if (stage == 0) {
        lv_event_send(save_button, LV_EVENT_CLICKED, NULL); stage = 1;
    } else if (stage == 1 && !child) {
        if (last_result) return -1;
        puts("TIME UI manual-save PASS");
        lv_event_send(sync_button, LV_EVENT_CLICKED, NULL); stage = 2;
    } else if (stage == 2 && !child) {
        if (last_result) return -1;
        puts("TIME UI network-sync PASS"); return 1;
    }
    return 0;
}
