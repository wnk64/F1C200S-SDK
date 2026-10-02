// SPDX-License-Identifier: GPL-2.0-only
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "shared/dbus-client.h"

static volatile sig_atomic_t running = 1;
static void stop(int signal_number) { (void)signal_number; running = 0; }

static int select_pcm(const struct ba_pcm *pcms, size_t length, const bdaddr_t *current)
{
    int selected = -1;
    unsigned int flags = BA_PCM_FLAG_SOURCE | BA_PCM_FLAG_PROFILE_A2DP;
    for (size_t i = 0; i < length; ++i) {
        if ((pcms[i].flags & flags) != flags || !pcms[i].sampling || !pcms[i].channels)
            continue;
        if (current && !bacmp(&pcms[i].addr, current))
            return (int)i;
        if (selected < 0 || memcmp(&pcms[i].addr, &pcms[selected].addr, sizeof(bdaddr_t)) < 0)
            selected = (int)i;
    }
    return selected;
}

static int atomic_write(const char *path, const char *content)
{
    char temporary[128];
    snprintf(temporary, sizeof(temporary), "%s.XXXXXX", path);
    int fd = mkstemp(temporary);
    if (fd < 0) return -1;
    FILE *file = fdopen(fd, "w");
    if (!file) { close(fd); unlink(temporary); return -1; }
    int result = fchmod(fd, 0644);
    if (fputs(content, file) == EOF) result = -1;
    if (fclose(file)) result = -1;
    if (!result) result = rename(temporary, path);
    if (result) unlink(temporary);
    return result;
}

static int set_route(const bdaddr_t *address)
{
    char mac[18], config[512], state[64];
    if (address) {
        ba2str(address, mac);
        snprintf(config, sizeof(config),
            "pcm.!default { type asym playback.pcm { type plug slave.pcm { "
            "type bluealsa service \"org.bluealsa\" device \"%s\" "
            "profile \"a2dp\" delay 20000 } } capture.pcm \"plughw:Codec,0\" }\n", mac);
        snprintf(state, sizeof(state), "bluetooth %s\n", mac);
    } else {
        strcpy(config, "pcm.!default { type plug slave.pcm \"hw:Codec,0\" }\n");
        strcpy(state, "headphone\n");
    }
    if (atomic_write("/run/audio-route.conf", config)) return -1;
    /* Invalidate libasound's cached global configuration for later PCM opens. */
    if (utimensat(AT_FDCWD, "/etc/asound.conf", NULL, 0)) return -1;
    if (atomic_write("/run/audio-route.state", state)) return -1;
    fprintf(stderr, "audio-route: %s", state);
    return 0;
}

static int self_test(void)
{
    struct ba_pcm p[3] = {0};
    bdaddr_t current;
    p[0].flags = BA_PCM_FLAG_SOURCE | BA_PCM_FLAG_PROFILE_SCO;
    p[1].flags = BA_PCM_FLAG_SOURCE | BA_PCM_FLAG_PROFILE_A2DP;
    p[2].flags = BA_PCM_FLAG_SINK | BA_PCM_FLAG_PROFILE_A2DP;
    for (int i = 0; i < 3; ++i) { p[i].sampling = 48000; p[i].channels = 2; }
    str2ba("12:34:56:78:90:01", &p[1].addr);
    if (select_pcm(p, 3, NULL) != 1 || select_pcm(p, 0, NULL) != -1) return 1;
    p[1].sampling = 0;
    if (select_pcm(p, 3, NULL) != -1) return 1;
    p[1].sampling = 48000;
    p[2].flags = p[1].flags;
    str2ba("12:34:56:78:90:02", &p[2].addr);
    current = p[2].addr;
    if (select_pcm(p, 3, &current) != 2) return 1;
    p[2].flags = 0;
    if (select_pcm(p, 3, &current) != 1) return 1;
    puts("route selection self-test passed");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--self-test")) return self_test();
    int watch = argc == 2 && !strcmp(argv[1], "--watch");
    if (!watch && !(argc == 2 && !strcmp(argv[1], "--once"))) {
        fputs("usage: f1-audio-route --watch|--once|--self-test\n", stderr); return 1;
    }
    signal(SIGTERM, stop); signal(SIGINT, stop);
    if (set_route(NULL)) { perror("initialize headphone fallback"); return 1; }
    bdaddr_t current = {{0}};
    int has_current = 0;
    do {
        struct ba_dbus_ctx ctx;
        struct ba_pcm *pcms = NULL;
        size_t length = 0;
        DBusError error = DBUS_ERROR_INIT;
        int selected = -1;
        if (bluealsa_dbus_connection_ctx_init(&ctx, "org.bluealsa", &error)) {
            if (bluealsa_dbus_get_pcms(&ctx, &pcms, &length, &error))
                selected = select_pcm(pcms, length, has_current ? &current : NULL);
            bluealsa_dbus_connection_ctx_free(&ctx);
        }
        if (selected < 0) {
            if (has_current && !set_route(NULL)) has_current = 0;
        } else if (!has_current || bacmp(&current, &pcms[selected].addr)) {
            if (!set_route(&pcms[selected].addr)) {
                current = pcms[selected].addr; has_current = 1;
            }
        }
        dbus_error_free(&error);
        free(pcms);
        if (watch && running) sleep(1);
    } while (watch && running);
    if (watch && has_current && set_route(NULL)) return 1;
    return 0;
}
