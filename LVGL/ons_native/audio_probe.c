#define _POSIX_C_SOURCE 200809L
#include "audio.h"
#include "events.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

static volatile sig_atomic_t stopped;
static void signal_stop(int number) { (void)number; stopped = 1; }
int main(int argc, char **argv)
{
    if (argc != 5) { fprintf(stderr, "usage: ons-audio-probe arc1.nsa resource.ogg alsa-device milliseconds\n"); return 1; }
    unsigned long duration = strtoul(argv[4], NULL, 10);
    if (!duration || duration > 600000) return 1;
    ons_audio audio; ons_audio_stats stats;
    if (ons_audio_open(&audio)) return 1;
    signal(SIGTERM, signal_stop); signal(SIGINT, signal_stop);
    ons_audio_volume(&audio, 60);
    if (ons_audio_start(&audio, argv[1], argv[2], argv[3], 0)) { ons_audio_close(&audio); return 1; }
    uint64_t begin = ons_monotonic_ms(), report = begin;
    while (!stopped && ons_monotonic_ms() - begin < duration) {
        ons_audio_snapshot(&audio, &stats);
        if (!stats.running) break;
        uint64_t now = ons_monotonic_ms();
        if (now - report >= 1000) {
            report = now;
            printf("Native audio: rate=%u channels=%u written=%llu underruns=%u holes=%u\n", stats.rate, stats.channels,
                (unsigned long long)stats.frames, stats.underruns, stats.holes); fflush(stdout);
        }
        struct timespec wait = {0, 10000000}; nanosleep(&wait, NULL);
    }
    ons_audio_stop(&audio); ons_audio_snapshot(&audio, &stats); ons_audio_close(&audio);
    printf("Native audio finished: result=%d frames=%llu rate=%u elapsed=%llu ms underruns=%u holes=%u\n", stats.result,
        (unsigned long long)stats.frames, stats.rate, (unsigned long long)stats.elapsed_ms, stats.underruns, stats.holes);
    return stats.result || !stats.frames ? 1 : 0;
}
