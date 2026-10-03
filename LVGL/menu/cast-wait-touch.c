#define _POSIX_C_SOURCE 200809L
#include "../touch/touch_orientation.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <signal.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
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
static int station_connected(int force)
{
    static unsigned long long checked;
    static int cached;
    unsigned long long now = milliseconds();
    if (!force && checked && now - checked < 250) return cached;
    checked = now;
    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return 1;
    struct sockaddr_un local = {.sun_family = AF_UNIX}, remote = {.sun_family = AF_UNIX};
    snprintf(local.sun_path + 1, sizeof(local.sun_path) - 1, "cast-touch-%ld", (long)getpid());
    snprintf(remote.sun_path, sizeof(remote.sun_path), "/tmp/manual_miracast_20260920/wpa_ctrl/wlan1");
    int rc = bind(fd, (struct sockaddr *)&local, offsetof(struct sockaddr_un, sun_path) + 1 + strlen(local.sun_path + 1));
    cached = 1; /* Unknown connection state must not cancel an active handshake. */
    if (!rc && !connect(fd, (struct sockaddr *)&remote, sizeof(remote)) && send(fd, "STA-FIRST", 9, 0) == 9) {
        struct pollfd event = {.fd = fd, .events = POLLIN};
        if (poll(&event, 1, 100) > 0 && (event.revents & POLLIN)) {
            char reply[1024]; ssize_t count = recv(fd, reply, sizeof(reply) - 1, 0);
            if (count > 0) {
                reply[count] = 0;
                if (!strncmp(reply, "FAIL", 4)) cached = 0;
            }
        }
    }
    close(fd); return cached;
}
static int connected(int force)
{
    FILE *file = fopen("/tmp/manual_miracast_20260920/sink.pid", "r");
    if (!file) return station_connected(force);
    long pid; int valid = fscanf(file, "%ld", &pid) == 1 && pid > 1; fclose(file);
    if (!valid || kill(pid, 0)) return station_connected(force);
    char path[80], executable[320];
    snprintf(path, sizeof(path), "/proc/%ld/exe", pid);
    ssize_t count = readlink(path, executable, sizeof(executable) - 1);
    if (count < 0) return station_connected(force);
    executable[count] = 0;
    return !strcmp(executable, "/root/aic_miracast/candidates/audio_reopen_20261002/sink-audio-reopen") || station_connected(force);
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
    if (argc == 2 && !strcmp(argv[1], "--connected")) return connected(1) ? 0 : 1;
    int simulated = argc == 2 && !strcmp(argv[1], "--simulate-triple");
    if (argc > 2 || (argc == 2 && !simulated)) return 1;
    signal(SIGTERM, stop); signal(SIGINT, stop);
    int fd = simulated ? -1 : open("/dev/i2c-0", O_RDWR | O_CLOEXEC);
    if (!simulated && fd < 0) { perror("cast touch input"); return 1; }
    struct gesture g = {0}; unsigned long long start = milliseconds();
    while (!stopped) {
        if (connected(0)) { if (fd >= 0) close(fd); puts("cast touch guard: connected/handshake or unknown, released"); return 0; }
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
        if (sample(&g, now, down, x, y) && !connected(1)) {
            if (fd >= 0) close(fd);
            puts("cast touch guard: triple-center exit requested"); return 10;
        }
delay: ;
        struct timespec pause = {0, 40000000}; nanosleep(&pause, NULL);
    }
    if (fd >= 0) close(fd);
    return 0;
}
