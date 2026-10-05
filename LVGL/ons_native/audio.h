#ifndef ONS_NATIVE_AUDIO_H
#define ONS_NATIVE_AUDIO_H
#include <stdint.h>
#include <pthread.h>
typedef struct {
    uint64_t frames, elapsed_ms;
    unsigned rate, channels, underruns, holes;
    int running, result;
} ons_audio_stats;
typedef struct {
    pthread_mutex_t lock;
    pthread_t worker;
    int joined, stop, paused, volume, loop;
    char archive[256], resource[512], device[128];
    ons_audio_stats stats;
} ons_audio;
int ons_audio_open(ons_audio *audio);
int ons_audio_start(ons_audio *audio, const char *archive, const char *resource, const char *device, int loop);
void ons_audio_volume(ons_audio *audio, unsigned percent);
void ons_audio_pause(ons_audio *audio, int paused);
void ons_audio_snapshot(ons_audio *audio, ons_audio_stats *stats);
void ons_audio_stop(ons_audio *audio);
void ons_audio_close(ons_audio *audio);
#endif
