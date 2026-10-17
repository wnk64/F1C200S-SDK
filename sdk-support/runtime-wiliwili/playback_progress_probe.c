#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static long milliseconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * 1000L + now.tv_nsec / 1000000;
}

/* Input URI stays on stdin, never in probe output. Run with an exclusive display. */
int main(int argc, char **argv) {
    char uri[16384];
    if ((argc != 4 && argc != 5) || !fgets(uri, sizeof(uri), stdin)) return 2;
    uri[strcspn(uri, "\r\n")] = 0;
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets)) return 2;
    pid_t child = fork();
    if (child < 0) return 2;
    if (!child) {
        close(sockets[0]);
        if (dup2(sockets[1], 3) < 0) _exit(125);
        if (sockets[1] != 3) close(sockets[1]);
        int log = open("/tmp/f1-wili-probe-player.log", O_CREAT | O_TRUNC | O_WRONLY, 0600);
        if (log < 0 || dup2(log, 1) < 0 || dup2(log, 2) < 0) _exit(125);
        if (log > 3) close(log);
        setenv("F1_OSD_EVENT_FD", "3", 1);
        setenv("CEDAR_ROTATE", "90", 1);
        setenv("CEDAR_AUDIODEV", "default", 1);
        setenv("F1_PLAYER_DEBUG", "1", 1);
        setenv("F1_PLAYER_START_MS", argv[2], 1);
        execl(argv[1], argv[1], uri, (char *)NULL);
        _exit(127);
    }
    close(sockets[1]);
    long started = milliseconds(), last = started, max_gap = 0;
    uint32_t first = 0, latest = 0;
    unsigned frames = 0;
    int status = 0, reaped = 0, passed = 0;
    while (milliseconds() - started < atoi(argv[3]) * 1000L) {
        struct pollfd event = {sockets[0], POLLIN, 0};
        if (poll(&event, 1, 100) > 0 && (event.revents & POLLIN)) {
            uint64_t value;
            if (recv(sockets[0], &value, sizeof(value), 0) == sizeof(value) && (uint32_t)value == 7) {
                uint32_t position = value >> 32;
                if (!frames || position > latest) {
                    long now = milliseconds(), gap = now - last;
                    if (!frames) { first = position; printf("first_ms=%u startup_ms=%ld\n", first, now - started); }
                    else if (gap > max_gap) max_gap = gap;
                    last = now; latest = position; ++frames;
                    uint32_t required = argc == 5 ? (uint32_t)atoi(argv[4]) * 1000U : 8000U;
                    if (latest >= first + required && frames >= 60) { passed = 1; break; }
                }
            }
        }
        if (waitpid(child, &status, WNOHANG) == child) { reaped = 1; break; }
    }
    if (!reaped) {
        kill(child, SIGTERM);
        for (int i = 0; i < 30; ++i) {
            if (waitpid(child, &status, WNOHANG) == child) { reaped = 1; break; }
            usleep(100000);
        }
        if (!reaped) { kill(child, SIGKILL); waitpid(child, &status, 0); }
    }
    close(sockets[0]);
    printf("%s seek_ms=%s frames=%u first=%u last=%u max_gap_ms=%ld stalled_ms=%ld\n",
           passed ? "PASS" : "FAIL", argv[2], frames, first, latest, max_gap, milliseconds() - last);
    return passed ? 0 : 1;
}
