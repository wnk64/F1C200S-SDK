#define _POSIX_C_SOURCE 200809L
#include "../touch/touch_orientation.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stopped;
static void stop(int signal_number) { (void)signal_number; stopped = 1; }
struct gesture { int down, valid, x, y, count; unsigned long long start, first; };
static unsigned long long milliseconds(void)
{
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
static int sample(struct gesture *g, unsigned long long now, int down, int x, int y)
{
    int center = x >= 140 && x < 340 && y >= 260 && y < 540;
    if (g->count && now - g->first > 2000) g->count = 0;
    if (down && !g->down) {
        g->start = now; g->x = x; g->y = y; g->valid = center;
        if (!center) g->count = 0;
    }
    if (down && (!center || abs(x - g->x) > 30 || abs(y - g->y) > 30)) {
        g->valid = 0; g->count = 0;
    }
    int hit = 0;
    if (!down && g->down) {
        if (g->valid && now - g->start >= 40 && now - g->start <= 500) {
            if (!g->count) g->first = now;
            hit = ++g->count == 3;
        } else g->count = 0;
    }
    g->down = down;
    return hit;
}
static int connected(void)
{
    FILE *file = fopen("/tmp/manual_miracast_20260920/sink.pid", "r");
    if (!file) return 0;
    long pid; int valid = fscanf(file, "%ld", &pid) == 1 && pid > 1; fclose(file);
    if (!valid || kill(pid, 0)) return 0;
    char path[80], executable[320];
    snprintf(path, sizeof(path), "/proc/%ld/exe", pid);
    ssize_t count = readlink(path, executable, sizeof(executable) - 1);
    if (count < 0) return 0;
    executable[count] = 0;
    return !strcmp(executable, "/root/aic_miracast/candidates/audio_reopen_20261002/sink-audio-reopen");
}
static int selftest(void)
{
    struct gesture g = {0}; int hit = 0;
    for (int i = 0; i < 3; ++i) {
        sample(&g, i * 200, 1, 240, 400);
        hit = sample(&g, i * 200 + 80, 0, 240, 400);
        if (hit != (i == 2)) return 1;
    }
    g = (struct gesture){0};
    sample(&g, 0, 1, 240, 400); if (sample(&g, 800, 0, 240, 400) || g.count) return 1;
    sample(&g, 1000, 1, 240, 400); sample(&g, 1040, 1, 280, 400);
    if (sample(&g, 1080, 0, 280, 400) || g.count) return 1;
    sample(&g, 1100, 1, 240, 400); sample(&g, 1180, 0, 240, 400);
    sample(&g, 4000, 1, 240, 400);
    if (sample(&g, 4080, 0, 240, 400) || g.count != 1) return 1;
    sample(&g, 4200, 1, 0, 0);
    if (sample(&g, 4280, 0, 0, 0) || g.count) return 1;
    puts("PASS: triple release, timeout, long hold, motion and outside reset"); return 0;
}
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--selftest")) return selftest();
    int simulated = argc == 2 && !strcmp(argv[1], "--simulate-triple");
    if (argc > 2 || (argc == 2 && !simulated)) return 1;
    signal(SIGTERM, stop); signal(SIGINT, stop);
    int fd = simulated ? -1 : open("/dev/i2c-0", O_RDWR | O_CLOEXEC);
    if (!simulated && fd < 0) { perror("cast touch input"); return 1; }
    struct gesture g = {0}; unsigned long long start = milliseconds();
    while (!stopped) {
        if (connected()) { if (fd >= 0) close(fd); puts("cast touch guard: connected, released"); return 0; }
        unsigned long long now = milliseconds();
        int down, x, y;
        if (simulated) {
            down = (now - start) % 240 < 120; x = 240; y = 400;
        } else {
            unsigned char reg = 2, report[13];
            struct i2c_msg messages[2] = {
                {.addr = 0x38, .len = 1, .buf = &reg},
                {.addr = 0x38, .flags = I2C_M_RD, .len = sizeof(report), .buf = report}
            };
            struct i2c_rdwr_ioctl_data transfer = {messages, 2};
            if (ioctl(fd, I2C_RDWR, &transfer) != 2 || (report[0] & 15) > 1) {
                g = (struct gesture){0}; goto delay;
            }
            down = (report[0] & 15) == 1;
            int rx = ((report[1] & 15) << 8) | report[2];
            int ry = ((report[3] & 15) << 8) | report[4];
            ft_touch_portrait(rx, ry, &x, &y);
        }
        if (sample(&g, now, down, x, y) && !connected()) {
            if (fd >= 0) close(fd);
            puts("cast touch guard: triple-center exit requested"); return 10;
        }
delay: ;
        struct timespec pause = {0, 40000000}; nanosleep(&pause, NULL);
    }
    if (fd >= 0) close(fd);
    return 0;
}
