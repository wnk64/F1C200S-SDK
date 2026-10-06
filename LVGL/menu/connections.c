#define _POSIX_C_SOURCE 200809L
#include "connections.h"
#include "net_protocol.h"
#include "ui.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

enum { CAPACITY = 32, ROWS = 3 };
struct device { char key[70], name[170]; int connected, known, secure, signal; };
static struct device devices[CAPACITY];
static int total, bt, list_page, selected, scene, input_pair, key_mode;
static pid_t worker;
static int output_fd = -1, input_fd = -1, listing, had_error, got_done;
static char failure_reason[64];
static char incoming[18000];
static size_t used;
static char helper_path[320];
static lv_obj_t *entry, *keyboard, *message;
static const char *letters[] = {"q","w","e","r","t","y","u","i","\n","o","p","a","s","d","f","g","h","\n","j","k","l","z","x","c","v","b","\n","ABC","123","n","m"," ",LV_SYMBOL_BACKSPACE,LV_SYMBOL_OK,""};
static const char *capitals[] = {"Q","W","E","R","T","Y","U","I","\n","O","P","A","S","D","F","G","H","\n","J","K","L","Z","X","C","V","B","\n","abc","123","N","M"," ",LV_SYMBOL_BACKSPACE,LV_SYMBOL_OK,""};
static const char *numbers[] = {"1","2","3","4","5","6","7","8","\n","9","0","-","_","@",".",",",":","\n","!","?","#","$","%","&","*","+","\n","abc","#+=","(",")"," ",LV_SYMBOL_BACKSPACE,LV_SYMBOL_OK,""};
static const char *symbols[] = {"~","`","^","=","[","]","{","}","\n",";",":","'","\"","\\","/","<",">","\n","!","?","#","$","%","&","*","+","\n","abc","123","(",")"," ",LV_SYMBOL_BACKSPACE,LV_SYMBOL_OK,""};
static void draw_list(void);
static void details(void);
static int start_job(const char *operation, const char *key, const char *secret);
static void status(const char *text)
{
    if (message && lv_obj_is_valid(message)) lv_label_set_text(message, text);
}
static void page_header(const char *title, void (*back)(void))
{
    header(title, 1); menu_back_to(back);
    message = menu_status();
    extern const lv_font_t menu_font_28;
    lv_obj_set_style_text_font(message, &menu_font_28, 0);
    entry = keyboard = NULL;
}
static void to_settings(void)
{
    if (worker) { status("请稍候"); return; }
    scene = 0; message = entry = keyboard = NULL; menu_settings();
}
static void to_list(void)
{
    if (worker) { status("请稍候"); return; }
    draw_list();
}
static void select_device(lv_event_t *event)
{
    if (worker) return;
    selected = (int)(intptr_t)lv_event_get_user_data(event); details();
}
static void page_change(lv_event_t *event)
{
    if (worker) return;
    int next = list_page + (int)(intptr_t)lv_event_get_user_data(event);
    if (next >= 0 && next < (total + ROWS - 1) / ROWS) { list_page = next; draw_list(); }
}
static void scan(lv_event_t *event)
{
    (void)event; if (!worker) start_job(bt ? "bt-scan" : "wifi-scan", NULL, NULL);
}
static void draw_list(void)
{
    scene = 1; page_header(bt ? "蓝牙" : "Wi-Fi", to_settings);
    lv_obj_t *root = menu_content();
    status(total ? "选择设备" : "暂无设备");
    for (int row = 0; row < ROWS && list_page * ROWS + row < total; row++) {
        int index = list_page * ROWS + row;
        struct device *d = &devices[index];
        lv_obj_t *b = button(root, 24, 168 + row * 152, 432, 132, 0xFFFFFF);
        lv_obj_t *name = label(b, d->name, 22, 20, 388, 0x172329);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        label(b, d->connected ? "已连接" : d->known ? (bt ? "已配对" : "已保存") : (bt ? "未配对" : "未连接"), 22, 74, 300, d->connected ? 0x167A6F : 0x66757E);
        lv_obj_add_event_cb(b, select_device, LV_EVENT_CLICKED, (void *)(intptr_t)index);
    }
    lv_obj_t *refresh = button(root, 24, 640, 432, 72, 0xFFFFFF);
    label(refresh, LV_SYMBOL_REFRESH "  扫描", 138, 18, 230, 0x167A6F);
    lv_obj_add_event_cb(refresh, scan, LV_EVENT_CLICKED, NULL);
    for (int side = 0; side < 2; side++) {
        lv_obj_t *b = button(root, side ? 368 : 24, 728, 88, 56, 0xFFFFFF);
        lv_obj_t *s = label(b, side ? LV_SYMBOL_RIGHT : LV_SYMBOL_LEFT, 0, 0, 60, 0x172329);
        lv_obj_center(s); lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_add_event_cb(b, page_change, LV_EVENT_CLICKED, (void *)(intptr_t)(side ? 1 : -1));
        if ((!side && !list_page) || (side && (list_page + 1) * ROWS >= total)) lv_obj_add_state(b, LV_STATE_DISABLED);
    }
    lv_obj_t *p = label(root, "", 156, 742, 168, 0x66757E);
    lv_label_set_text_fmt(p, "%d / %d", list_page + 1, total ? (total + ROWS - 1) / ROWS : 1);
    lv_obj_set_style_text_align(p, LV_TEXT_ALIGN_CENTER, 0);
}
static void input_page(int pair);
static void connect_device(lv_event_t *event)
{
    (void)event; if (worker) return;
    struct device *d = &devices[selected];
    if (!bt && !d->known && d->secure == 2) { status("不支持此认证"); return; }
    if (!bt && !d->known && d->secure) { input_page(0); return; }
    start_job(bt ? "bt-connect" : "wifi-connect", d->key, bt || d->known ? NULL : "");
}
static void disconnect_device(lv_event_t *event)
{
    (void)event; if (!worker) start_job(bt ? "bt-disconnect" : "wifi-disconnect", devices[selected].key, NULL);
}
static void forget_yes(lv_event_t *event)
{
    (void)event; if (!worker) start_job(bt ? "bt-forget" : "wifi-forget", devices[selected].key, NULL);
}
static void cancel_forget(lv_event_t *event) { (void)event; if (!worker) details(); }
static void forget_device(lv_event_t *event)
{
    (void)event; if (worker) return;
    scene = 3; page_header("忘记设备", details);
    label(menu_content(), devices[selected].name, 28, 192, 424, 0x172329);
    label(menu_content(), "忘记此设备？", 28, 288, 424, 0xB84545);
    lv_obj_t *yes = button(menu_content(), 24, 416, 432, 100, 0xFFFFFF);
    label(yes, "确认忘记", 140, 32, 250, 0xB84545); lv_obj_add_event_cb(yes, forget_yes, LV_EVENT_CLICKED, NULL);
    lv_obj_t *no = button(menu_content(), 24, 540, 432, 100, 0xFFFFFF);
    label(no, "取消", 186, 32, 200, 0x172329); lv_obj_add_event_cb(no, cancel_forget, LV_EVENT_CLICKED, NULL);
}
static void details(void)
{
    scene = 2; page_header(bt ? "蓝牙设备" : "Wi-Fi", to_list);
    struct device *d = &devices[selected];
    lv_obj_t *root = menu_content();
    label(root, d->name, 28, 182, 424, 0x172329);
    label(root, bt ? d->key : d->secure ? "加密网络" : "开放网络", 28, 268, 424, 0x66757E);
    status(d->connected ? "已连接" : d->known ? (bt ? "已配对" : "已保存") : "未连接");
    lv_obj_t *b = button(root, 24, 390, 432, 108, 0xFFFFFF);
    label(b, d->connected ? "断开连接" : "连接", d->connected ? 150 : 184, 36, 260, 0x167A6F);
    lv_obj_add_event_cb(b, d->connected ? disconnect_device : connect_device, LV_EVENT_CLICKED, NULL);
    b = button(root, 24, 528, 432, 108, 0xFFFFFF);
    label(b, "忘记设备", 150, 36, 260, 0xB84545);
    lv_obj_add_event_cb(b, forget_device, LV_EVENT_CLICKED, NULL);
    if (!bt && !d->known) lv_obj_add_state(b, LV_STATE_DISABLED);
}
static void send_pair(const char *answer)
{
    if (input_fd >= 0) {
        (void)write(input_fd, answer, strlen(answer)); (void)write(input_fd, "\n", 1);
    }
}
static void input_submit(void)
{
    if (!entry) return;
    char secret[80]; snprintf(secret, sizeof(secret), "%s", lv_textarea_get_text(entry));
    if (input_pair) {
        if (!secret[0]) { memset(secret, 0, sizeof(secret)); return; }
        send_pair(secret); lv_textarea_set_text(entry, "");
        page_header("蓝牙设备", to_list); scene = 2; status("正在连接");
    } else {
        char check[150];
        if (net_psk(secret, check, sizeof(check))) { status("密码需为8至63位"); memset(secret, 0, sizeof(secret)); return; }
        lv_textarea_set_text(entry, ""); details(); start_job("wifi-connect", devices[selected].key, secret);
        memset(check, 0, sizeof(check));
    }
    memset(secret, 0, sizeof(secret));
}
static void keyboard_event(lv_event_t *event)
{
    (void)event;
    const char *key = lv_btnmatrix_get_btn_text(keyboard, lv_btnmatrix_get_selected_btn(keyboard));
    if (!key) return;
    if (!strcmp(key, "ABC")) key_mode = 1;
    else if (!strcmp(key, "abc")) key_mode = 0;
    else if (!strcmp(key, "123")) key_mode = 2;
    else if (!strcmp(key, "#+=")) key_mode = 3;
    else if (!strcmp(key, LV_SYMBOL_BACKSPACE)) { lv_textarea_del_char(entry); return; }
    else if (!strcmp(key, LV_SYMBOL_OK)) { input_submit(); return; }
    else { lv_textarea_add_text(entry, key); return; }
    lv_btnmatrix_set_map(keyboard, key_mode == 1 ? capitals : key_mode == 2 ? numbers : key_mode == 3 ? symbols : letters);
}
static void input_page(int pair)
{
    input_pair = pair; scene = 4; key_mode = 0;
    page_header(pair ? "配对码" : "Wi-Fi密码", pair ? to_list : details);
    label(menu_content(), devices[selected].name, 28, 164, 424, 0x172329);
    entry = lv_textarea_create(menu_content()); lv_obj_set_pos(entry, 24, 252); lv_obj_set_size(entry, 432, 72);
    lv_textarea_set_one_line(entry, true); lv_textarea_set_max_length(entry, pair ? 16 : 64);
    lv_textarea_set_password_mode(entry, !pair);
    lv_obj_set_style_text_font(entry, &lv_font_montserrat_24, 0);
    keyboard = lv_btnmatrix_create(menu_content()); lv_obj_set_pos(keyboard, 12, 388); lv_obj_set_size(keyboard, 456, 380);
    lv_btnmatrix_set_map(keyboard, letters); lv_btnmatrix_set_one_checked(keyboard, false);
    lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_24, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(keyboard, 4, 0); lv_obj_set_style_pad_row(keyboard, 8, 0); lv_obj_set_style_pad_column(keyboard, 4, 0);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(0xD5DFE3), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_add_event_cb(keyboard, keyboard_event, LV_EVENT_VALUE_CHANGED, NULL);
}
static void pair_choice(lv_event_t *event)
{
    send_pair((int)(intptr_t)lv_event_get_user_data(event) ? "yes" : "no");
    page_header("蓝牙设备", to_list); scene = 2; status("正在连接");
}
static void pair_prompt(const char *type, const char *code)
{
    if (!strcmp(type, "PIN")) { input_page(1); return; }
    page_header("确认配对", to_list); scene = 5;
    label(menu_content(), devices[selected].name, 28, 192, 424, 0x172329);
    label(menu_content(), code, 28, 284, 424, 0x167A6F);
    if (!strcmp(type, "DISPLAY")) { status("配对码"); return; }
    lv_obj_t *yes = button(menu_content(), 24, 416, 432, 100, 0xFFFFFF);
    label(yes, "确认", 186, 32, 200, 0x167A6F); lv_obj_add_event_cb(yes, pair_choice, LV_EVENT_CLICKED, (void *)1);
    lv_obj_t *no = button(menu_content(), 24, 540, 432, 100, 0xFFFFFF);
    label(no, "取消", 186, 32, 200, 0xB84545); lv_obj_add_event_cb(no, pair_choice, LV_EVENT_CLICKED, NULL);
}
static int start_job(const char *operation, const char *key, const char *secret)
{
    if (worker) return -1;
    if (!helper_path[0]) {
        ssize_t n = readlink("/proc/self/exe", helper_path, sizeof(helper_path)-1);
        if (n < 0) return -1;
        helper_path[n] = 0; char *slash = strrchr(helper_path, '/'); if (!slash) return -1;
        snprintf(slash + 1, sizeof(helper_path) - (size_t)(slash + 1 - helper_path), "connections-helper-ip");
    }
    int output[2], input[2]; if (pipe(output)) return -1;
    if (pipe(input)) { close(output[0]); close(output[1]); return -1; }
    listing = strstr(operation, "-scan") || strstr(operation, "-list");
    if (listing) total = 0;
    used = 0; had_error = got_done = 0;failure_reason[0]=0;
    worker = fork();
    if (worker < 0) { close(output[0]); close(output[1]); close(input[0]); close(input[1]); worker = 0; status("启动失败"); return -1; }
    if (!worker) {
        dup2(input[0], STDIN_FILENO); dup2(output[1], STDOUT_FILENO);
        close(output[0]); close(output[1]); close(input[0]); close(input[1]);
        int log = open("/root/aic_miracast/candidates/lvgl_connections_20261002/connections.log", O_CREAT | O_WRONLY | O_APPEND, 0600);
        if (log >= 0) { dup2(log, STDERR_FILENO); close(log); }
        for (int fd = 3; fd < 64; fd++) close(fd);
        if (key) execl(helper_path, helper_path, operation, key, (char *)NULL);
        else execl(helper_path, helper_path, operation, (char *)NULL);
        _exit(127);
    }
    close(output[1]); close(input[0]); output_fd = output[0]; input_fd = input[1];
    fcntl(output_fd, F_SETFL, O_NONBLOCK);
    if (secret) { (void)write(input_fd, secret, strlen(secret)); (void)write(input_fd, "\n", 1); }
    if (!bt) { close(input_fd); input_fd = -1; }
    status(listing ? "正在扫描" : "正在处理"); return 0;
}
static void result_line(char *line)
{
    char *save = NULL, *type = strtok_r(line, "\t", &save);
    if (!type) return;
    if (!strcmp(type, "DEVICE") && total < CAPACITY) {
        char *fields[6]; for (int i = 0; i < 6; i++) fields[i] = strtok_r(NULL, "\t", &save);
        if (!fields[5] || strlen(fields[0]) >= sizeof(devices[0].key)) return;
        struct device *d = &devices[total];
        if (net_unhex(fields[1], d->name, sizeof(d->name))) return;
        for (char *p = d->name; *p; p++) if ((unsigned char)*p < 32) *p = ' ';
        snprintf(d->key, sizeof(d->key), "%s", fields[0]);
        d->connected = atoi(fields[2]); d->known = atoi(fields[3]); d->secure = atoi(fields[4]); d->signal = atoi(fields[5]); total++;
    } else if (!strcmp(type, "DONE")) got_done = 1;
    else if (!strcmp(type, "ERROR")) {had_error=1;char *why=strtok_r(NULL,"\t",&save);if(why)snprintf(failure_reason,sizeof(failure_reason),"%s",why);}
    else if (!strcmp(type,"STATUS")) status("正在恢复 Wi-Fi，请稍候");
    else if (!strcmp(type, "CONFIRM") || !strcmp(type, "PIN") || !strcmp(type, "DISPLAY")) {
        char *code = strtok_r(NULL, "\t", &save); pair_prompt(type, code ? code : "");
    }
}
void connections_poll(void)
{
    if (!worker) return;
    char buffer[1200]; ssize_t n;
    while ((n = read(output_fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            if (buffer[i] == '\n') { incoming[used] = 0; result_line(incoming); used = 0; }
            else if (used + 1 < sizeof(incoming)) incoming[used++] = buffer[i];
            else had_error = 1;
        }
    }
    int code;
    pid_t finished = waitpid(worker, &code, WNOHANG);
    if (finished <= 0) return;
    close(output_fd); output_fd = -1;
    if (input_fd >= 0) close(input_fd);
    input_fd = -1;
    worker = 0;
    int failed = had_error || !got_done || !WIFEXITED(code) || WEXITSTATUS(code);
    if (listing) { list_page = 0; draw_list(); if (failed) status("扫描失败，请重试"); }
    else if (failed) {
        details();
        if(!strcmp(failure_reason,"connect-timeout"))status("关联失败，请检查热点或密码");
        else if(!strcmp(failure_reason,"ip-timeout")||!strcmp(failure_reason,"ip-configure-failed"))status("已连接，但未获取网络地址");
        else if(!strcmp(failure_reason,"wifi-unavailable"))status("Wi-Fi 服务不可用");
        else status("操作失败，请重试");
    }
    else { draw_list(); start_job(bt ? "bt-list" : "wifi-list", NULL, NULL); }
}
void connections_show(int bluetooth)
{
    if (worker) return;
    bt = bluetooth; total = list_page = 0; draw_list(); start_job(bt ? "bt-scan" : "wifi-scan", NULL, NULL);
}
void connections_close(void)
{
    if (entry && lv_obj_is_valid(entry)) lv_textarea_set_text(entry, "");
    if (worker) {
        kill(worker, SIGTERM);
        int code, done = 0;
        for (int i = 0; i < 15; i++) {
            if (waitpid(worker, &code, WNOHANG) == worker) { done = 1; break; }
            struct timespec pause = {0, 100000000}; nanosleep(&pause, NULL);
        }
        if (!done) { kill(worker, SIGKILL); waitpid(worker, &code, 0); }
        char socket_path[100]; snprintf(socket_path, sizeof(socket_path), "/run/f1-menu-wpa-%ld", (long)worker);
        unlink(socket_path);
        worker = 0;
    }
    if (output_fd >= 0) close(output_fd);
    if (input_fd >= 0) close(input_fd);
    output_fd = input_fd = -1;
}
void connections_smoke(int bluetooth)
{
    int test = bluetooth;
    bt = test == 1 || test >= 4; total = 4; list_page = 0;
    for (int i = 0; i < total; i++) {
        snprintf(devices[i].name, sizeof(devices[i].name), "%s %d", bt ? "Bluetooth speaker" : "Wi-Fi network", i+1);
        snprintf(devices[i].key, sizeof(devices[i].key), "%s", bt ? "12:11:71:41:9C:4A" : "74657374");
        devices[i].known = i == 0; devices[i].connected = i == 0; devices[i].secure = !bt;
    }
    draw_list();
    selected = 1;
    if (test == 2 || test == 4) details();
    else if (test == 3) input_page(0);
    else if (test == 5) pair_prompt("CONFIRM", "123456");
}
