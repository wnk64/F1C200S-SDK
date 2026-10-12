#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stopping;
static void stop(int sig) { (void)sig; stopping = 1; }
static long long milliseconds(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
static int remove_runtime(const char *path, const struct stat *st, int type, struct FTW *walk)
{
    (void)st; (void)type; (void)walk;
    return remove(path);
}
static void signal_adopted_children(int sig)
{
    DIR *proc = opendir("/proc");
    if (!proc) return;
    struct dirent *entry;
    while ((entry = readdir(proc))) {
        char *end;
        long pid = strtol(entry->d_name, &end, 10);
        if (*end || pid <= 1 || pid == getpid()) continue;
        char path[64], data[1024];
        snprintf(path, sizeof(path), "/proc/%ld/stat", pid);
        FILE *file = fopen(path, "r");
        if (!file) continue;
        char *line = fgets(data, sizeof(data), file);
        fclose(file);
        if (!line) continue;
        char *tail = strrchr(data, ')'), state;
        long parent;
        if (tail && sscanf(tail + 1, " %c %ld", &state, &parent) == 2 && parent == getpid())
            kill((pid_t)pid, sig);
    }
    closedir(proc);
}
static int drain_children(pid_t group)
{
    long long started = milliseconds();
    if (group > 1) kill(-group, SIGTERM);
    for (;;) {
        int status;
        pid_t child;
        while ((child = waitpid(-1, &status, WNOHANG)) > 0) {}
        if (child < 0 && errno == ECHILD) return 0;
        long long age = milliseconds() - started;
        int sig = age < 500 ? SIGTERM : SIGKILL;
        if (group > 1) kill(-group, sig);
        /* Subreaping catches double-forked/setsid children outside the original group. */
        signal_adopted_children(sig);
        if (age > 3000) return -1;
        usleep(20000);
    }
}
int main(int argc, char **argv)
{
    if (argc < 2 || argv[1][0] != '/') return 125;
    if (prctl(PR_SET_CHILD_SUBREAPER, 1)) { perror("app subreaper"); return 125; }
    struct sigaction action = {0};
    action.sa_handler = stop;
    sigemptyset(&action.sa_mask);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
    char runtime[] = "/tmp/f1-app.XXXXXX";
    if (!mkdtemp(runtime)) return 125;
    pid_t supervisor = getpid(), player = fork();
    if (!player) {
        signal(SIGTERM, SIG_DFL); signal(SIGINT, SIG_DFL);
        if (setsid() < 0 || prctl(PR_SET_PDEATHSIG, SIGKILL) || getppid() != supervisor) _exit(125);
        setenv("TMPDIR", runtime, 1);
        setenv("F1_APP_RUNTIME_DIR", runtime, 1);
        execv(argv[1], argv + 1);
        perror("app exec"); _exit(127);
    }
    int result = 127, status;
    if (player > 0) {
        while (!stopping) {
            pid_t reaped = waitpid(-1, &status, 0);
            if (reaped == player) {
                result = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
                break;
            }
            if (reaped < 0 && errno != EINTR) break;
        }
        if (stopping) result = 143;
    }
    int failed = drain_children(player);
    if (!failed && nftw(runtime, remove_runtime, 16, FTW_DEPTH | FTW_PHYS | FTW_MOUNT)) failed = 1;
    fprintf(stderr, "app lifecycle: result=%d descendants=%s runtime=%s\n",
            result, failed ? "NOT_RELEASED" : "released", runtime);
    return failed ? 125 : result;
}
