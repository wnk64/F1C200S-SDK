#define _POSIX_C_SOURCE 200809L
#include "ip-settings.h"
#include "ui.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
LV_FONT_DECLARE(menu_font_28);
static lv_obj_t *mode, *inputs[3], *selected, *save;
static pid_t child;
static int active, last_result = -1;
static const char *keys[] = {"1", "2", "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n", ".", "0", "/", "\n", LV_SYMBOL_BACKSPACE, "", NULL};
void ip_settings_close(void)
{
    active = 0;
    if (child > 0) {
        kill(child, SIGTERM);
        while (waitpid(child, NULL, 0) < 0 && errno == EINTR) {}
        child = 0;
    }
}
static void focus(lv_event_t *event) { selected = lv_event_get_target(event); }
static void key(lv_event_t *event)
{
    lv_obj_t *matrix = lv_event_get_target(event);
    const char *text = lv_btnmatrix_get_btn_text(matrix, lv_btnmatrix_get_selected_btn(matrix));
    if (!selected || !text) return;
    if (!strcmp(text, LV_SYMBOL_BACKSPACE)) lv_textarea_del_char(selected);
    else lv_textarea_add_text(selected, text);
}
static void submit(lv_event_t *event)
{
    (void)event;
    if (child > 0) return;
    const char *choice = lv_obj_has_state(mode, LV_STATE_CHECKED) ? "static" : "dhcp";
    child = fork();
    if (!child) {
        int fd = open("/run/f1-ip-settings.status", O_CREAT | O_TRUNC | O_WRONLY, 0600);
        if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); close(fd); }
        execl("/opt/f1-network/f1-ip-config", "f1-ip-config", "configure", choice,
            lv_textarea_get_text(inputs[0]), lv_textarea_get_text(inputs[1]),
            lv_textarea_get_text(inputs[2]), (char *)NULL);
        _exit(127);
    }
    if (child < 0) { child = 0; lv_label_set_text(menu_status(), "启动失败"); return; }
    lv_obj_add_state(save, LV_STATE_DISABLED);
    last_result = -1;
    lv_label_set_text(menu_status(), "正在应用网络设置");
}
void ip_settings_show(void)
{
    header("IP 地址设置", 1); menu_back_to(menu_settings); active = 1;
    lv_obj_set_style_text_font(menu_status(), &menu_font_28, 0);
    lv_label_set_text(menu_status(), "当前 Wi-Fi 网络");
    char values[4][64] = {"dhcp", "10.0.0.234/24", "10.0.0.1", "10.0.0.1"};
    FILE *file = fopen("/etc/f1-ip.conf", "r");
    if (file) {
        for (int i = 0; i < 4; ++i) {
            if (!fgets(values[i], sizeof(values[i]), file)) break;
            values[i][strcspn(values[i], "\r\n")] = 0;
        }
        fclose(file);
    }
    label(menu_content(), "静态 IP", 28, 166, 180, 0x172329);
    mode = lv_switch_create(menu_content()); lv_obj_set_pos(mode, 344, 158); lv_obj_set_size(mode, 108, 52);
    lv_obj_set_style_radius(mode, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(mode, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(mode, lv_color_hex(0xA6B3BA), 0);
    lv_obj_set_style_bg_opa(mode, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(mode, lv_color_hex(0x167A6F), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_radius(mode, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(mode, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(mode, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_radius(mode, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(mode, -4, LV_PART_KNOB);
    if (!strcmp(values[0], "static")) lv_obj_add_state(mode, LV_STATE_CHECKED);
    const char *names[] = {"IP / CIDR", "网关", "DNS"};
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *name = label(menu_content(), names[i], 28, 236 + i * 62, 120, 0x66757E);
        if (i != 1) lv_obj_set_style_text_font(name, &lv_font_montserrat_16, 0);
        inputs[i] = lv_textarea_create(menu_content());
        lv_obj_set_pos(inputs[i], 148, 220 + i * 62); lv_obj_set_size(inputs[i], 304, 56);
        lv_obj_set_style_text_font(inputs[i], &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(inputs[i], lv_color_hex(0x172329), 0);
        lv_obj_set_style_bg_color(inputs[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(inputs[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(inputs[i], 1, 0);
        lv_obj_set_style_border_color(inputs[i], lv_color_hex(0xDDE3E7), 0);
        lv_obj_set_style_border_color(inputs[i], lv_color_hex(0x167A6F), LV_STATE_FOCUSED);
        lv_obj_set_style_radius(inputs[i], 8, 0);
        lv_obj_set_style_pad_all(inputs[i], 8, 0);
        lv_textarea_set_one_line(inputs[i], true); lv_textarea_set_max_length(inputs[i], 24);
        lv_textarea_set_accepted_chars(inputs[i], "0123456789./");
        lv_textarea_set_text(inputs[i], values[i + 1]);
        lv_obj_add_event_cb(inputs[i], focus, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(inputs[i], focus, LV_EVENT_CLICKED, NULL);
    }
    selected = inputs[0];
    lv_obj_t *keyboard = lv_btnmatrix_create(menu_content());
    lv_obj_set_pos(keyboard, 24, 426); lv_obj_set_size(keyboard, 432, 252);
    lv_btnmatrix_set_map(keyboard, keys);
    lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_24, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS);
    lv_obj_set_style_pad_row(keyboard, 6, 0);
    lv_obj_set_style_pad_column(keyboard, 6, 0);
    lv_obj_set_style_border_width(keyboard, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(keyboard, lv_color_hex(0xDDE3E7), LV_PART_ITEMS);
    lv_obj_set_style_radius(keyboard, 8, LV_PART_ITEMS);
    lv_obj_add_event_cb(keyboard, key, LV_EVENT_VALUE_CHANGED, NULL);
    save = button(menu_content(), 24, 700, 432, 76, 0xFFFFFF);
    label(save, LV_SYMBOL_SAVE, 28, 20, 64, 0x167A6F);
    label(save, "应用", 112, 20, 272, 0x172329);
    lv_obj_add_event_cb(save, submit, LV_EVENT_CLICKED, NULL);
}
void ip_settings_poll(void)
{
    if (!active || child <= 0) return;
    int status;
    if (waitpid(child, &status, WNOHANG) != child) return;
    child = 0; lv_obj_clear_state(save, LV_STATE_DISABLED);
    int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    last_result = rc;
    lv_label_set_text(menu_status(), rc == 0 ? "网络设置已应用" : rc == 3 ?
        "IP 参数错误" : rc == 4 ? "Wi-Fi 未连接" : "应用失败，请检查日志");
}
int ip_settings_test_step(void)
{
    static int started;
    if (!started) {
        started = 1;
        lv_event_send(save, LV_EVENT_CLICKED, NULL);
    }
    if (child > 0) return 0;
    return last_result == 0 ? 1 : -1;
}
