#ifndef F1_DLNA_PROCESS_H
#define F1_DLNA_PROCESS_H
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
static void dlna_child_session(void *unused)
{
    (void)unused;
    if (setsid() < 0) _exit(127);
}
static void dlna_signal_group(pid_t group, int signal_number)
{
    if (group > 1) kill(-group, signal_number);
}
#endif
