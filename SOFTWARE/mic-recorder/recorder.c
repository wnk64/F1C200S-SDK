#define _GNU_SOURCE
#include "platform.h"
#include <alsa/asoundlib.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern const lv_font_t menu_font_stream;
void menu_font_stream_close(void);
enum { RATE = 16000, MAX_BYTES = RATE * 2 * 3600 };
static const char *directory = "/root/roms/Recordings";
static snd_pcm_t *pcm;
static snd_ctl_t *ctl;
static snd_ctl_elem_value_t *saved[8];
static const char *controls[] = {
    "ADC Mixer Left FM In Capture Switch", "ADC Mixer Left Out Capture Switch",
    "ADC Mixer Line In Capture Switch", "ADC Mixer Mic Capture Switch",
    "ADC Mixer Right FM In Capture Switch", "ADC Mixer Right Out Capture Switch",
    "ADC Capture Volume", "Mic Boost Volume"
};
static int fd = -1, peak, max_peak, failed, xruns;
static uint32_t bytes;
static double energy;
static char path[512], message[160] = "就绪";
static pid_t playback;
static uint64_t last_data;
static volatile sig_atomic_t quit;
static lv_obj_t *status_label, *time_label, *file_label, *level_label;
static void stop_signal(int sig) { (void)sig; quit = 1; }
static void error_text(const char *operation, int err)
{
    snprintf(message, sizeof(message), "%s: %s", operation, snd_strerror(err));
    fprintf(stderr, "%s\n", message);
}
static int write_all(int file, const void *data, size_t length)
{
    const char *p = data;
    while (length) {
        ssize_t n = write(file, p, length);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        p += n; length -= n;
    }
    return 0;
}
static void little32(unsigned char *p, uint32_t v)
{ for (int i = 0; i < 4; i++) p[i] = v >> (i * 8); }
static int wav_header(void)
{
    unsigned char h[44] = {0};
    memcpy(h, "RIFF", 4); little32(h + 4, bytes + 36);
    memcpy(h + 8, "WAVEfmt ", 8); little32(h + 16, 16);
    h[20] = 1; h[22] = 1; little32(h + 24, RATE);
    little32(h + 28, RATE * 2); h[32] = 2; h[34] = 16;
    memcpy(h + 36, "data", 4); little32(h + 40, bytes);
    return lseek(fd, 0, SEEK_SET) < 0 ? -1 : write_all(fd, h, sizeof(h));
}
static void restore_mixer(void)
{
    if (ctl) {
        for (int i = 0; i < 8; i++) if (saved[i]) {
            int rc = snd_ctl_elem_write(ctl, saved[i]);
            if (rc < 0) fprintf(stderr, "restore %s: %s\n", controls[i], snd_strerror(rc));
            snd_ctl_elem_value_free(saved[i]); saved[i] = NULL;
        }
        snd_ctl_close(ctl); ctl = NULL;
    }
}
static int setup_mixer(void)
{
    int rc = snd_ctl_open(&ctl, "hw:Codec", 0);
    if (rc < 0) return rc;
    /* Save all values before changing any route; never touch playback controls. */
    for (int i = 0; i < 8; i++) {
        rc = snd_ctl_elem_value_malloc(&saved[i]);
        if (rc < 0) return rc;
        snd_ctl_elem_value_set_interface(saved[i], SND_CTL_ELEM_IFACE_MIXER);
        snd_ctl_elem_value_set_name(saved[i], controls[i]);
        rc = snd_ctl_elem_read(ctl, saved[i]);
        if (rc < 0) { snd_ctl_elem_value_free(saved[i]); saved[i] = NULL; return rc; }
    }
    snd_ctl_elem_value_t *value;
    rc = snd_ctl_elem_value_malloc(&value);
    if (rc < 0) return rc;
    for (int i = 0; i < 8; i++) {
        snd_ctl_elem_value_copy(value, saved[i]);
        if (i < 6) snd_ctl_elem_value_set_boolean(value, 0, i == 3);
        else snd_ctl_elem_value_set_integer(value, 0, i == 6 ? 3 : 1);
        rc = snd_ctl_elem_write(ctl, value);
        if (rc < 0) break;
    }
    snd_ctl_elem_value_free(value); return rc < 0 ? rc : 0;
}
static void stop_playback(void)
{
    if (playback > 0) {
        kill(playback, SIGTERM);
        for (int i = 0; i < 50; i++) {
            if (waitpid(playback, NULL, WNOHANG) != 0) { playback = 0; return; }
            usleep(10000);
        }
        kill(playback, SIGKILL); waitpid(playback, NULL, 0); playback = 0;
    }
}
static void finish_record(void)
{
    if (pcm) { snd_pcm_drop(pcm); snd_pcm_close(pcm); pcm = NULL; }
    if (fd >= 0) {
        if (wav_header() || fsync(fd)) { error_text("保存失败", -errno); failed = 1; }
        if (close(fd)) { error_text("关闭失败", -errno); failed = 1; }
        fd = -1;
        double rms = bytes ? sqrt(energy / (bytes / 2)) : 0;
        fprintf(stderr, "MIC bytes=%u seconds=%.3f peak=%d rms=%.2f xruns=%d failed=%d file=%s\n",
                bytes, bytes / (double)(RATE * 2), max_peak, rms, xruns, failed, path);
        if (!failed) snprintf(message, sizeof(message), "已保存");
    }
    restore_mixer();
}
static int begin_record(const char *requested)
{
    if (pcm) return -1;
    stop_playback();
    struct stat sd, root;
    struct statvfs space;
    if (stat("/root/roms", &sd) || stat("/root", &root) || sd.st_dev == root.st_dev ||
        statvfs("/root/roms", &space) || (uint64_t)space.f_bavail * space.f_frsize < 1048576) {
        snprintf(message, sizeof(message), "SD 卡不可用或空间不足"); return -1;
    }
    if (mkdir(directory, 0755) && errno != EEXIST) { error_text("目录失败", -errno); return -1; }
    bytes = 0; peak = max_peak = xruns = failed = 0; energy = 0;
    if (requested) snprintf(path, sizeof(path), "%s", requested);
    else {
        time_t now = time(NULL); struct tm tm; char date[40]; localtime_r(&now, &tm);
        strftime(date, sizeof(date), "%Y%m%d-%H%M%S", &tm);
        snprintf(path, sizeof(path), "%s/rec-%s-%ld.wav", directory, date, (long)getpid());
    }
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0) { error_text("文件失败", -errno); return -1; }
    if (wav_header()) goto file_error;
    int rc = setup_mixer();
    if (rc < 0) { error_text("MIC 通道失败", rc); goto abort_record; }
    rc = snd_pcm_open(&pcm, "hw:Codec,0", SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
    if (rc < 0) { pcm = NULL; error_text("录音设备失败", rc); goto abort_record; }
    rc = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 1, RATE, 0, 160000);
    if (rc < 0) { error_text("录音参数失败", rc); goto abort_record; }
    rc = snd_pcm_start(pcm);
    if (rc < 0) { error_text("启动采样失败", rc); goto abort_record; }
    last_data = platform_ms();
    snprintf(message, sizeof(message), "录音中"); return 0;
file_error:
    error_text("写入失败", -errno);
abort_record:
    failed = 1; finish_record(); return -1;
}
static void capture(void)
{
    if (!pcm) return;
    if (platform_ms() - last_data > 3000) {
        error_text("录音无数据", -ETIMEDOUT); failed = 1; finish_record(); return;
    }
    int16_t samples[1024];
    /* Bound work per UI tick so a faulty capture device cannot starve touch. */
    for (int iteration = 0; iteration < 4 && pcm; iteration++) {
        snd_pcm_sframes_t n = snd_pcm_readi(pcm, samples, 1024);
        if (n == -EAGAIN || !n) return;
        if (n == -EPIPE) {
            xruns++; int rc = snd_pcm_prepare(pcm);
            if (rc >= 0) rc = snd_pcm_start(pcm);
            if (rc >= 0 && xruns < 10) { last_data = platform_ms(); return; }
            error_text("录音溢出", rc < 0 ? rc : -EPIPE); failed = 1; finish_record(); return;
        }
        if (n < 0) { error_text("录音失败", n); failed = 1; finish_record(); return; }
        uint32_t amount = (uint32_t)n * 2;
        if (write_all(fd, samples, amount)) { error_text("SD 写入失败", -errno); failed = 1; finish_record(); return; }
        bytes += amount;
        last_data = platform_ms();
        peak = 0;
        for (int i = 0; i < n; i++) {
            int v = abs((int)samples[i]); if (v > peak) peak = v;
            energy += (double)samples[i] * samples[i];
        }
        if (peak > max_peak) max_peak = peak;
        if (bytes >= MAX_BYTES) { finish_record(); return; }
    }
}
static void clicked(lv_event_t *event)
{
    long action = (long)lv_event_get_user_data(event);
    if (action == 0) { if (pcm) finish_record(); else begin_record(NULL); }
    else if (action == 1) {
        if (pcm) finish_record();
        if (playback) { stop_playback(); snprintf(message, sizeof(message), "已停止回放"); }
        else if (*path && bytes && !failed) {
            playback = fork();
            if (!playback) { execlp("aplay", "aplay", "-D", "default", path, (char *)NULL); _exit(127); }
            if (playback < 0) { playback = 0; error_text("回放失败", -errno); }
            else snprintf(message, sizeof(message), "回放中");
        } else snprintf(message, sizeof(message), "暂无录音");
    } else quit = 1;
}
static lv_obj_t *label(const char *text, int y)
{
    lv_obj_t *o = lv_label_create(lv_scr_act()); lv_obj_set_pos(o, 24, y); lv_obj_set_width(o, 432);
    lv_obj_set_style_text_font(o, &menu_font_stream, 0); lv_label_set_text(o, text); return o;
}
static void button(const char *text, int y, long action)
{
    lv_obj_t *o = lv_btn_create(lv_scr_act()); lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, 24, y); lv_obj_set_size(o, 432, 88);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(o, lv_color_white(), 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(0xdbeae2), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(o, 2, 0); lv_obj_set_style_border_color(o, lv_color_hex(0x58666b), 0);
    lv_obj_set_style_radius(o, 4, 0);
    lv_obj_t *l = lv_label_create(o); lv_obj_set_style_text_font(l, &menu_font_stream, 0);
    lv_label_set_text(l, text); lv_obj_center(l); lv_obj_add_event_cb(o, clicked, LV_EVENT_CLICKED, (void *)action);
}
int main(int argc, char **argv)
{
    signal(SIGINT, stop_signal); signal(SIGTERM, stop_signal);
    int headless = argc == 4 && !strcmp(argv[1], "--record");
    unsigned seconds = headless ? strtoul(argv[2], NULL, 10) : 0;
    if (argc != 1 && (!headless || !seconds || seconds > 3600)) return 1;
    if (headless) {
        if (begin_record(argv[3])) return 1;
        while (!quit && pcm && bytes < seconds * RATE * 2) { capture(); usleep(10000); }
        finish_record(); return failed ? 1 : 0;
    }
    if (platform_open()) return 1;
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xf1f4f6), 0);
    lv_obj_set_style_text_color(lv_scr_act(), lv_color_hex(0x172329), 0);
    label("麦克风录音", 32); status_label = label(message, 110);
    time_label = label("00:00", 178); level_label = label("MIC 0%", 244);
    file_label = label("SD / Recordings", 312); lv_label_set_long_mode(file_label, LV_LABEL_LONG_WRAP);
    button("录音 / 停止保存", 448, 0); button("回放 / 停止", 550, 1); button("返回", 652, 2);
    uint64_t tick = platform_ms(), refreshed = 0;
    while (!quit) {
        capture();
        if (playback > 0) {
            int status; pid_t p = waitpid(playback, &status, WNOHANG);
            if (p == playback) { playback = 0; snprintf(message, sizeof(message), WIFEXITED(status) && !WEXITSTATUS(status) ? "回放结束" : "回放失败"); }
        }
        uint64_t now = platform_ms(); lv_tick_inc(now - tick); tick = now;
        if (now - refreshed >= 200) {
            refreshed = now; char text[80]; unsigned sec = bytes / (RATE * 2);
            lv_label_set_text(status_label, message); snprintf(text, sizeof(text), "%02u:%02u", sec / 60, sec % 60);
            lv_label_set_text(time_label, text); snprintf(text, sizeof(text), "MIC %d%%", peak * 100 / 32768);
            lv_label_set_text(level_label, text);
            lv_label_set_text(file_label, *path ? strrchr(path, '/') ? strrchr(path, '/') + 1 : path : "SD / Recordings");
        }
        lv_timer_handler(); usleep(10000);
    }
    finish_record(); stop_playback(); platform_close(); menu_font_stream_close(); return 0;
}
