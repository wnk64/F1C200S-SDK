#define _GNU_SOURCE
#include "media_display.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

extern const lv_font_t menu_font_stream;
void menu_font_stream_close(void);
static pid_t player;
static int command_fd = -1, reply_fd = -1, frame_fd = -1;
static volatile sig_atomic_t quit;
static double duration, position, seek_target;
static int paused, volume = 100, shown, dragging, seeking, ready, failed;
static int reported_volume = -1;
static uint64_t activity, seek_started;
static lv_obj_t *controls, *play_button, *play_label, *seek_slider, *volume_slider, *time_label;
static void visibility(int visible)
{
    shown = visible;
    if (visible) lv_obj_clear_flag(controls, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(controls, LV_OBJ_FLAG_HIDDEN);
    media_osd_present(visible);
}
static void stop(int signal) { (void)signal; quit = 1; }
static int send_command(const char *command)
{
    size_t length = strlen(command);
    if (command_fd < 0 || length > 250 || write(command_fd, command, length) != (ssize_t)length) return -1;
    return 0;
}
static int value(const char *line, const char *prefix, double *number)
{
    size_t size = strlen(prefix); if (strncmp(line, prefix, size)) return 0;
    char *end; double parsed = strtod(line + size, &end);
    if (end == line + size || *end || !isfinite(parsed) || parsed < 0 || parsed > 86400) return -1;
    *number = parsed; return 1;
}
static void response(const char *line)
{
    double number;
    if (value(line, "ANS_LENGTH=", &number) == 1) duration = number;
    else if (value(line, "ANS_volume=", &number) == 1 && number <= 100) {
        reported_volume = (int)number;
        if (!lv_slider_is_dragged(volume_slider)) {
            volume = (int)number; lv_slider_set_value(volume_slider, volume, LV_ANIM_OFF);
        }
    }
}
static void read_player(void)
{
    static char line[512]; static unsigned length;
    char bytes[2048]; ssize_t count;
    while ((count = read(reply_fd, bytes, sizeof(bytes))) > 0)
        for (ssize_t i = 0; i < count; ++i) {
            if (bytes[i] == '\n' || bytes[i] == '\r') {
                line[length] = 0; if (length) response(line); length = 0;
            } else if (length + 1 < sizeof(line)) line[length++] = bytes[i];
        }
    uint64_t event;
    while (recv(frame_fd, &event, sizeof(event), MSG_DONTWAIT) == sizeof(event)) {
        if ((event & 0xffffffffu) != 7) continue;
        double seconds = (event >> 32) / 1000.0;
        if (seeking && (seconds < seek_target - 1.0 || seconds > seek_target + 5.0)) continue;
        if (seeking) { seeking = 0; fprintf(stderr, "MPLAYER UI seek ready=%.3f target=%.3f\n", seconds, seek_target); }
        if (!ready) fprintf(stderr, "MPLAYER UI first actual frame=%.3f\n", seconds);
        ready = 1;
        if (!dragging) position = seconds;
    }
}
static void touch_activity(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_PRESSED || lv_event_get_code(event) == LV_EVENT_PRESSING)
        activity = platform_ms();
}
static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int width)
{
    lv_obj_t *object = lv_label_create(parent); lv_obj_set_pos(object, x, y); lv_obj_set_width(object, width);
    lv_obj_set_style_text_font(object, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(object, lv_color_white(), 0); lv_label_set_text(object, text);
    return object;
}
static lv_obj_t *button(int x, const char *symbol)
{
    lv_obj_t *object = lv_btn_create(controls); lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, x, 10); lv_obj_set_size(object, 60, 66);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(0x243c44), 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(0x42616a), LV_STATE_PRESSED);
    lv_obj_set_style_radius(object, 4, 0);
    lv_obj_add_event_cb(object, touch_activity, LV_EVENT_ALL, NULL);
    lv_obj_t *text = label(object, symbol, 0, 0, 60);
    lv_obj_set_style_text_font(text, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0); lv_obj_center(text);
    return object;
}
static void exit_clicked(lv_event_t *event) { (void)event; quit = 1; }
static void play_clicked(lv_event_t *event)
{
    (void)event; activity = platform_ms();
    if (!seeking && !send_command("pause\n")) paused = !paused;
    lv_label_set_text(play_label, paused ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
}
static void seek_event(lv_event_t *event)
{
    int code = lv_event_get_code(event); activity = platform_ms();
    if (code == LV_EVENT_PRESSED && duration > 0 && ready && !seeking) {
        dragging = 1;
        if (!paused && !send_command("pause\n")) paused = 1;
        lv_label_set_text(play_label, LV_SYMBOL_PLAY);
    }
    if (code == LV_EVENT_VALUE_CHANGED && dragging)
        position = duration * lv_slider_get_value(seek_slider) / 1000;
    if (code == LV_EVENT_RELEASED && dragging) {
        char command[80]; seek_target = duration * lv_slider_get_value(seek_slider) / 1000;
        snprintf(command, sizeof(command), "seek %.3f 2\n", seek_target);
        dragging = 0;
        if (!send_command(command)) {
            seeking = 1; seek_started = platform_ms(); paused = 0;
            lv_label_set_text(play_label, LV_SYMBOL_PAUSE);
        }
    }
}
static void volume_changed(lv_event_t *event)
{
    (void)event; activity = platform_ms();
    char command[64]; volume = lv_slider_get_value(volume_slider);
    snprintf(command, sizeof(command), "pausing_keep_force volume %d 1\n", volume);
    send_command(command);
}
static void center_clicked(lv_event_t *event)
{
    if (lv_event_get_target(event) != lv_scr_act()) return;
    lv_point_t point; lv_indev_get_point(lv_indev_get_act(), &point);
    if (point.x < 250 || point.x > 550 || point.y < 140 || point.y > 340) return;
    visibility(!shown); activity = platform_ms();
}
static lv_obj_t *slider(int x, int width, int max)
{
    lv_obj_t *object = lv_slider_create(controls); lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, x, 28); lv_obj_set_size(object, width, 12); lv_slider_set_range(object, 0, max);
    lv_obj_set_ext_click_area(object, 18);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(object, lv_color_hex(0x59676b), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(object, lv_color_hex(0x25b69d), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(object, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_pad_all(object, 7, LV_PART_KNOB);
    lv_obj_add_event_cb(object, touch_activity, LV_EVENT_ALL, NULL);
    return object;
}
static void build_ui(void)
{
    lv_obj_t *root = lv_scr_act(); lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0); lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(root, center_clicked, LV_EVENT_CLICKED, NULL);
    controls = lv_obj_create(root); lv_obj_remove_style_all(controls);
    lv_obj_set_pos(controls, 0, 394); lv_obj_set_size(controls, 800, 86);
    lv_obj_clear_flag(controls, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(controls, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(controls, lv_color_hex(0x182b32), 0);
    lv_obj_t *back = button(8, LV_SYMBOL_LEFT); lv_obj_add_event_cb(back, exit_clicked, LV_EVENT_CLICKED, NULL);
    play_button = button(80, LV_SYMBOL_PAUSE); play_label = lv_obj_get_child(play_button, 0);
    lv_obj_add_event_cb(play_button, play_clicked, LV_EVENT_CLICKED, NULL);
    seek_slider = slider(166, 376, 1000); lv_obj_add_event_cb(seek_slider, seek_event, LV_EVENT_ALL, NULL);
    time_label = label(controls, "00:00 / --:--", 166, 54, 376);
    lv_obj_t *speaker = label(controls, LV_SYMBOL_VOLUME_MAX, 566, 22, 40);
    lv_obj_set_style_text_font(speaker, &lv_font_montserrat_24, 0);
    volume_slider = slider(624, 148, 100); lv_slider_set_value(volume_slider, 100, LV_ANIM_OFF);
    lv_obj_add_event_cb(volume_slider, volume_changed, LV_EVENT_VALUE_CHANGED, NULL);
    label(controls, "MPlayer", 624, 54, 148);
    lv_obj_add_flag(controls, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(root); lv_refr_now(NULL);
}
static int start_player(const char *executable, const char *file)
{
    int input[2] = {-1, -1}, output[2] = {-1, -1}, events[2] = {-1, -1};
    if (pipe2(input, O_CLOEXEC) || pipe2(output, O_CLOEXEC) ||
        socketpair(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, events)) {
        for (unsigned i = 0; i < 2; ++i) {
            if (input[i] >= 0) close(input[i]);
            if (output[i] >= 0) close(output[i]);
            if (events[i] >= 0) close(events[i]);
        }
        return -1;
    }
    player = fork();
    if (player == 0) {
        dup2(input[0], STDIN_FILENO); dup2(output[1], STDOUT_FILENO); dup2(events[1], 3);
        fcntl(3, F_SETFD, 0);
        setenv("MPLAYER_OSD_FD", "3", 1); setenv("CEDAR_ROTATE", "90", 1);
        setenv("CEDAR_CMA_POOL_MB", "12", 1); setenv("TERM", "dumb", 1);
        const char *loop = getenv("MPLAYER_UI_LOOP");
        execl(executable, executable, "-slave", "-noconsolecontrols", "-quiet", "-noconfig", "all",
            "-softvol", "-softvol-max", "100", "-afm", "faad", "-vc", "cedarh264", "-vo", "cedar_drm:native",
            "-ao", "alsa", "-loop", loop && !strcmp(loop, "1") ? "0" : "1", file, (char *)NULL);
        _exit(127);
    }
    close(input[0]); close(output[1]); close(events[1]);
    if (player < 0) { close(input[1]); close(output[0]); close(events[0]); return -1; }
    command_fd = input[1]; reply_fd = output[0]; frame_fd = events[0];
    fcntl(command_fd, F_SETFL, O_NONBLOCK); fcntl(reply_fd, F_SETFL, O_NONBLOCK);
    return 0;
}
static int close_player(void)
{
    if (player <= 0) return 0;
    send_command("quit\n"); int status;
    for (unsigned i = 0; i < 300; ++i) {
        pid_t ended = waitpid(player, &status, WNOHANG);
        if (ended == player || (ended < 0 && errno == ECHILD)) { player = -1; return 0; }
        if (i == 100) kill(player, SIGTERM);
        if (i == 200) kill(player, SIGKILL);
        usleep(10000);
    }
    return -1;
}
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--selftest-parser")) {
        double number;
        assert(value("ANS_LENGTH=35.25", "ANS_LENGTH=", &number) == 1 && number == 35.25);
        assert(value("ANS_LENGTH=nan", "ANS_LENGTH=", &number) == -1);
        assert(value("ANS_LENGTH=-1", "ANS_LENGTH=", &number) == -1);
        assert(value("ANS_LENGTH=12junk", "ANS_LENGTH=", &number) == -1);
        assert(value("other=12", "ANS_LENGTH=", &number) == 0);
        puts("MPlayer UI reply parser PASS"); return 0;
    }
    if (argc != 3) { fprintf(stderr, "usage: mplayer-ui media-file backend-executable\n"); return 1; }
    signal(SIGTERM, stop); signal(SIGINT, stop); signal(SIGPIPE, SIG_IGN);
    if (platform_open()) { platform_close(); return 2; }
    build_ui();
    if (media_osd_open() || start_player(argv[2], argv[1])) {
        media_osd_close(); platform_close(); return 1;
    }
    uint64_t tick = platform_ms(), query = 0, present = 0, update = 0, started = tick;
    const char *selftest_env = getenv("MPLAYER_UI_SELFTEST");
    int selftest = selftest_env && !strcmp(selftest_env, "1"), stage = 0;
    uint64_t stage_started = tick;
    double paused_position = 0;
    activity = tick;
    while (!quit) {
        uint64_t now = platform_ms(); lv_tick_inc(now - tick); tick = now;
        read_player();
        int status; pid_t ended = waitpid(player, &status, WNOHANG);
        if (ended == player) {
            fprintf(stderr, "MPLAYER UI child exit=%d\n", status); player = -1;
            failed = !WIFEXITED(status) || WEXITSTATUS(status); break;
        }
        if (now - query >= 500) {
            send_command("pausing_keep_force get_time_length\npausing_keep_force get_property volume\n"); query = now;
        }
        if (shown && !dragging && now - activity >= 10000) visibility(0);
        if (now - update >= 250) {
        update = now;
        if (!dragging && duration > 0 && !seeking)
            lv_slider_set_value(seek_slider, (int)fmin(1000, position * 1000 / duration), LV_ANIM_OFF);
        char clock[96];
        if (seeking || !ready) snprintf(clock, sizeof(clock), "Loading...");
        else snprintf(clock, sizeof(clock), "%02u:%02u / %02u:%02u", (unsigned)position / 60,
            (unsigned)position % 60, (unsigned)duration / 60, (unsigned)duration % 60);
        if (strcmp(lv_label_get_text(time_label), clock)) lv_label_set_text(time_label, clock);
        }
        if (!ready && now - started > 15000) { fprintf(stderr, "MPlayer UI startup has no actual frame\n"); failed = 1; break; }
        if (seeking && now - seek_started > 15000) {
            fprintf(stderr, "MPLAYER UI seek timed out without a target frame\n"); failed = 1; break;
        }
        lv_timer_handler();
        if (now - present >= 100) { media_osd_present(shown); present = now; }
        if (selftest) {
            if (stage == 0 && ready && duration > 0 && now - started >= 2000) {
                visibility(1); lv_event_send(play_button, LV_EVENT_CLICKED, NULL);
                stage = 1; stage_started = now;
            } else if (stage == 1 && now - stage_started >= 1000) {
                paused_position = position; stage = 2; stage_started = now;
            } else if (stage == 2 && now - stage_started >= 1000) {
                if (fabs(position - paused_position) > 0.1) { fprintf(stderr, "UI pause advanced video position\n"); failed = 1; break; }
                fprintf(stderr, "MPLAYER UI pause position stable=%.3f\n", position);
                lv_slider_set_value(volume_slider, 30, LV_ANIM_OFF);
                lv_event_send(volume_slider, LV_EVENT_VALUE_CHANGED, NULL);
                stage = 3; stage_started = now;
            } else if (stage == 3 && now - stage_started >= 1000) {
                if (reported_volume != 30) { fprintf(stderr, "UI volume not acknowledged=%d\n", reported_volume); failed = 1; break; }
                fprintf(stderr, "MPLAYER UI volume acknowledged=30\n");
                lv_event_send(seek_slider, LV_EVENT_PRESSED, NULL);
                lv_slider_set_value(seek_slider, 500, LV_ANIM_OFF);
                lv_event_send(seek_slider, LV_EVENT_VALUE_CHANGED, NULL);
                lv_event_send(seek_slider, LV_EVENT_RELEASED, NULL);
                stage = 4; stage_started = now;
            } else if (stage == 4 && !seeking && now - stage_started > 1000) {
                visibility(0); fprintf(stderr, "MPLAYER UI pause/volume/seek/frame-ready/hide PASS\n"); quit = 1;
            }
            if (now - started > 25000) { fprintf(stderr, "UI selftest timeout\n"); failed = 1; break; }
        }
        usleep(5000);
    }
    if (close_player()) return 2;
    if (command_fd >= 0) close(command_fd);
    if (reply_fd >= 0) close(reply_fd);
    if (frame_fd >= 0) close(frame_fd);
    media_osd_close(); platform_close(); menu_font_stream_close();
    return failed ? 1 : 0;
}
