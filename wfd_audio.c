#include "wfd_lpcm.h"
#include <alsa/asoundlib.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef AUDIO_ROUTE_STATE
#define AUDIO_ROUTE_STATE "/run/audio-route.state"
#endif
#ifndef AUDIO_RETRY_MS
#define AUDIO_RETRY_MS 500
#endif

#define PCM_QUEUE_BYTES 16384u
struct audio_state {
    struct wfd_lpcm demux;
    uint8_t queue[PCM_QUEUE_BYTES];
    size_t head, count;
    pthread_mutex_t lock;
    pthread_cond_t ready;
    pthread_t thread;
    atomic_int stop;
    int failed;
    char device[128];
    unsigned long long received, written, dropped, underruns;
    unsigned long long opens, retries, route_changes;
};
static struct audio_state *audio;

static void pcm_arrived(void *opaque, const uint8_t *data, size_t bytes, int64_t pts)
{
    struct audio_state *s = opaque;
    pthread_mutex_lock(&s->lock);
    if (!s->received)
        fprintf(stderr, "LPCM: PID=1100 S16_BE 48000Hz stereo first_pts=%lld bytes=%zu\n",
                (long long)pts, bytes);
    s->received += bytes;
    if (atomic_load(&s->stop) || s->failed) {
        s->dropped += bytes;
        pthread_mutex_unlock(&s->lock);
        return;
    }
    if (bytes > PCM_QUEUE_BYTES) {
        s->dropped += bytes - PCM_QUEUE_BYTES;
        data += bytes - PCM_QUEUE_BYTES;
        bytes = PCM_QUEUE_BYTES;
    }
    if (s->count + bytes > PCM_QUEUE_BYTES) {
        size_t discard = s->count + bytes - PCM_QUEUE_BYTES;
        s->head = (s->head + discard) % PCM_QUEUE_BYTES;
        s->count -= discard;
        s->dropped += discard;
    }
    size_t tail = (s->head + s->count) % PCM_QUEUE_BYTES;
    size_t first = PCM_QUEUE_BYTES - tail;
    if (first > bytes)
        first = bytes;
    memcpy(s->queue + tail, data, first);
    memcpy(s->queue, data + first, bytes - first);
    s->count += bytes;
    pthread_cond_signal(&s->ready);
    pthread_mutex_unlock(&s->lock);
}

static int64_t audio_now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static void close_pcm(snd_pcm_t **pcm)
{
    if (*pcm) {
        snd_pcm_drop(*pcm);
        snd_pcm_close(*pcm);
        *pcm = NULL;
    }
}

static void retry_wait(struct audio_state *s, int64_t until, int ignore_data)
{
    pthread_mutex_lock(&s->lock);
    while (!atomic_load(&s->stop) && (ignore_data || !s->count)) {
        int64_t left = until - audio_now_ms();
        if (left <= 0)
            break;
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += left / 1000;
        deadline.tv_nsec += (left % 1000) * 1000000;
        if (deadline.tv_nsec >= 1000000000) {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000000000;
        }
        pthread_cond_timedwait(&s->ready, &s->lock, &deadline);
    }
    pthread_mutex_unlock(&s->lock);
}

static int route_changed(char last[128])
{
    char current[128] = "unavailable";
    FILE *f = fopen(AUDIO_ROUTE_STATE, "r");
    if (f) {
        if (!fgets(current, sizeof(current), f))
            strcpy(current, "unavailable");
        fclose(f);
    }
    if (!strcmp(last, current))
        return 0;
    int changed = last[0] != '\0';
    strcpy(last, current);
    return changed;
}

static void *audio_thread(void *opaque)
{
    struct audio_state *s = opaque;
    snd_pcm_t *pcm = NULL;
    uint8_t block[4096];
    size_t bytes = 0, offset = 0;
    int error = 0, last_error = 0;
    int64_t next_route = 0, next_open = 0;
    char last_route[128] = "";
    while (!atomic_load(&s->stop)) {
        int64_t now = audio_now_ms();
        if (!strcmp(s->device, "default") && now >= next_route) {
            next_route = now + AUDIO_RETRY_MS;
            if (route_changed(last_route)) {
                close_pcm(&pcm);
                pthread_mutex_lock(&s->lock);
                s->dropped += s->count + bytes - offset;
                s->count = 0;
                s->failed = 0;
                s->route_changes++;
                pthread_mutex_unlock(&s->lock);
                bytes = offset = 0;
                next_open = 0;
                fprintf(stderr, "LPCM default route changed; reopen audio only\n");
            }
        }
        if (!pcm && now < next_open) {
            retry_wait(s, next_open, 1);
            continue;
        }
        pthread_mutex_lock(&s->lock);
        while (!s->received && !atomic_load(&s->stop))
            pthread_cond_wait(&s->ready, &s->lock);
        pthread_mutex_unlock(&s->lock);
        if (atomic_load(&s->stop))
            break;
        if (!pcm) {
            if (!strcmp(s->device, "default"))
                snd_config_update_free_global();
            error = snd_pcm_open(&pcm, s->device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
            if (error < 0)
                goto unavailable;
            /* ALSA plug handles endian conversion for Codec or BlueALSA. */
            error = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_BE,
                                      SND_PCM_ACCESS_RW_INTERLEAVED, 2, 48000, 1, 40000);
            if (error < 0)
                goto unavailable;
            pthread_mutex_lock(&s->lock);
            s->failed = 0;
            s->opens++;
            pthread_mutex_unlock(&s->lock);
            last_error = 0;
            fprintf(stderr, "LPCM output: device=%s S16_BE 48000Hz stereo latency=40ms\n", s->device);
        }
        if (offset == bytes) {
            pthread_mutex_lock(&s->lock);
            if (!s->count && !atomic_load(&s->stop)) {
                pthread_mutex_unlock(&s->lock);
                retry_wait(s, audio_now_ms() + AUDIO_RETRY_MS, 0);
                continue;
            }
            bytes = s->count < sizeof(block) ? s->count : sizeof(block);
            offset = 0;
            size_t first = PCM_QUEUE_BYTES - s->head;
            if (first > bytes)
                first = bytes;
            memcpy(block, s->queue + s->head, first);
            memcpy(block + first, s->queue, bytes - first);
            s->head = (s->head + bytes) % PCM_QUEUE_BYTES;
            s->count -= bytes;
            pthread_mutex_unlock(&s->lock);
        }
        if (atomic_load(&s->stop))
            break;
        if (offset < bytes) {
            snd_pcm_sframes_t frames = snd_pcm_writei(pcm, block + offset, (bytes - offset) / 4);
            if (frames == -EAGAIN || frames == 0) {
                error = snd_pcm_wait(pcm, 50);
                if (error < 0 && snd_pcm_recover(pcm, error, 1) < 0)
                    goto unavailable;
                continue;
            }
            if (frames < 0) {
                pthread_mutex_lock(&s->lock);
                s->underruns++;
                pthread_mutex_unlock(&s->lock);
                error = snd_pcm_recover(pcm, (int)frames, 1);
                if (error < 0)
                    goto unavailable;
                continue;
            }
            offset += (size_t)frames * 4;
            pthread_mutex_lock(&s->lock);
            s->written += (unsigned long long)frames * 4;
            pthread_mutex_unlock(&s->lock);
        }
        continue;
unavailable:
        close_pcm(&pcm);
        if (error != last_error) {
            fprintf(stderr, "LPCM output unavailable, retrying in %dms: %s\n",
                    AUDIO_RETRY_MS, snd_strerror(error));
            last_error = error;
        }
        pthread_mutex_lock(&s->lock);
        s->failed = 1;
        s->retries++;
        s->dropped += s->count + bytes - offset;
        s->count = 0;
        pthread_mutex_unlock(&s->lock);
        bytes = offset = 0;
        next_open = audio_now_ms() + AUDIO_RETRY_MS;
    }
    close_pcm(&pcm);
    pthread_mutex_lock(&s->lock);
    s->dropped += s->count + bytes - offset;
    s->count = 0;
    pthread_mutex_unlock(&s->lock);
    return NULL;
}

int wfd_audio_start(const char *device)
{
    if (audio || !device || strlen(device) >= sizeof(audio->device))
        return -1;
    struct audio_state *s = calloc(1, sizeof(*s));
    if (!s)
        return -1;
    strcpy(s->device, device);
    atomic_init(&s->stop, 0);
    if (pthread_mutex_init(&s->lock, NULL)) {
        free(s);
        return -1;
    }
    if (pthread_cond_init(&s->ready, NULL)) {
        pthread_mutex_destroy(&s->lock);
        free(s);
        return -1;
    }
    wfd_lpcm_init(&s->demux, pcm_arrived, s);
    pthread_attr_t attr;
    int result = pthread_attr_init(&attr);
    int attr_initialized = !result;
    if (!result)
        result = pthread_attr_setstacksize(&attr, 64 * 1024);
    if (!result)
        result = pthread_create(&s->thread, &attr, audio_thread, s);
    if (attr_initialized)
        pthread_attr_destroy(&attr);
    if (result) {
        pthread_cond_destroy(&s->ready);
        pthread_mutex_destroy(&s->lock);
        free(s);
        return -1;
    }
    audio = s;
    fprintf(stderr, "LPCM enabled: bounded queue=%u PES=%u stack=65536; device=%s\n",
            PCM_QUEUE_BYTES, WFD_PES_MAX, device);
    return 0;
}

void wfd_audio_ts(const uint8_t *ts)
{
    if (audio)
        wfd_lpcm_ts(&audio->demux, ts);
}

void wfd_audio_report(void)
{
    if (!audio)
        return;
    struct audio_state *s = audio;
    pthread_mutex_lock(&s->lock);
    fprintf(stderr, "LPCM stats: PES=%lu bytes=%llu written=%llu queue=%zu dropped=%llu underruns=%llu malformed=%lu discontinuities=%lu opens=%llu retries=%llu route_changes=%llu unavailable=%d\n",
            s->demux.completed, s->received, s->written, s->count, s->dropped,
            s->underruns, s->demux.malformed, s->demux.discontinuities,
            s->opens, s->retries, s->route_changes, s->failed);
    pthread_mutex_unlock(&s->lock);
}

void wfd_audio_stop(void)
{
    if (!audio)
        return;
    struct audio_state *s = audio;
    atomic_store(&s->stop, 1);
    pthread_mutex_lock(&s->lock);
    pthread_cond_broadcast(&s->ready);
    pthread_mutex_unlock(&s->lock);
    pthread_join(s->thread, NULL);
    wfd_audio_report();
    pthread_cond_destroy(&s->ready);
    pthread_mutex_destroy(&s->lock);
    audio = NULL;
    free(s);
}
