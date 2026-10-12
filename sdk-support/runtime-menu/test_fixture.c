#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
int main(int argc, char **argv)
{
    assert(argc == 3);
    const char *runtime = getenv("F1_APP_RUNTIME_DIR");
    assert(runtime && strstr(runtime, "/tmp/f1-app.") == runtime);
    int record = open(argv[2], O_CREAT | O_WRONLY | O_TRUNC, 0600);
    assert(record >= 0);
    dprintf(record, "%s\n", runtime);
    char path[128];
    snprintf(path, sizeof(path), "%s/fixture", runtime);
    int file = open(path, O_CREAT | O_WRONLY, 0600);
    assert(file >= 0); close(file);
    int channel[2]; assert(pipe(channel) == 0);
    pid_t child = fork(); assert(child >= 0);
    if (!child) {
        close(channel[0]); assert(setsid() >= 0);
        pid_t nested = fork(); assert(nested >= 0);
        if (!nested) { close(channel[1]); close(record); for (;;) pause(); }
        dprintf(record, "%ld\n%ld\n", (long)getpid(), (long)nested);
        assert(write(channel[1], "1", 1) == 1); close(channel[1]); close(record);
        for (;;) pause();
    }
    close(channel[1]); char ready;
    assert(read(channel[0], &ready, 1) == 1); close(channel[0]); close(record);
    if (!strcmp(argv[1], "crash")) kill(getpid(), SIGKILL);
    if (!strcmp(argv[1], "wait")) for (;;) pause();
    return 0;
}
