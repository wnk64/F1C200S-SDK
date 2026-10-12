#define _GNU_SOURCE
#include "dlna_process.h"
#include <assert.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/wait.h>
int main(void)
{
    int channel[2], status;
    assert(prctl(PR_SET_CHILD_SUBREAPER, 1) == 0);
    assert(pipe(channel) == 0);
    pid_t player = fork();
    assert(player >= 0);
    if (!player) {
        close(channel[0]);
        dlna_child_session(NULL);
        pid_t cache = fork();
        if (!cache) { close(channel[1]); for (;;) pause(); }
        assert(write(channel[1], &cache, sizeof(cache)) == sizeof(cache));
        for (;;) pause();
    }
    close(channel[1]);
    pid_t cache;
    assert(read(channel[0], &cache, sizeof(cache)) == sizeof(cache));
    close(channel[0]);
    assert(getpgid(player) == player && getpgid(cache) == player);
    kill(player, SIGKILL);
    assert(waitpid(player, &status, 0) == player && WIFSIGNALED(status));
    dlna_signal_group(player, SIGTERM);
    assert(waitpid(cache, &status, 0) == cache && WIFSIGNALED(status));
    dlna_signal_group(player, SIGKILL);
    dlna_signal_group(0, SIGTERM);
    dlna_signal_group(1, SIGTERM);
    puts("DLNA orphan-cache process-group cleanup PASS");
    return 0;
}
