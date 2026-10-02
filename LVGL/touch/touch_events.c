#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int fd = open(argc > 1 ? argv[1] : "/dev/input/event0", O_RDONLY | O_NONBLOCK);
    if (fd < 0) { perror("open evdev"); return 1; }
    char name[128] = {0};
    struct input_absinfo x, y;
    if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0 ||
        ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &x) < 0 ||
        ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &y) < 0) {
        perror("touch capability"); close(fd); return 1;
    }
    printf("name=%s x=%d..%d y=%d..%d\n", name, x.minimum, x.maximum, y.minimum, y.maximum);
    fflush(stdout);
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    unsigned frames = 0, events = 0;
    for (;;) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - start.tv_sec >= 30) break;
        struct pollfd p = {fd, POLLIN, 0};
        int ready = poll(&p, 1, 500);
        if (ready < 0) { if (errno == EINTR) continue; perror("poll"); close(fd); return 1; }
        if (!ready) continue;
        if (p.revents & (POLLERR | POLLHUP | POLLNVAL)) { close(fd); return 1; }
        struct input_event event;
        while (read(fd, &event, sizeof(event)) == sizeof(event)) {
            events++;
            if (event.type == EV_SYN && event.code == SYN_REPORT) frames++;
            printf("type=%u code=%u value=%d\n", event.type, event.code, event.value);
        }
        fflush(stdout);
    }
    printf("events=%u frames=%u\n", events, frames);
    close(fd);
    return 0;
}
