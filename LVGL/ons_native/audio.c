#define _POSIX_C_SOURCE 200809L
#include "audio.h"
#include "native.h"
#include "events.h"
#include <vorbis/vorbisfile.h>
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

struct stream { FILE *file; uint32_t offset, length, position; };
static size_t stream_read(void *output, size_t size, size_t count, void *argument)
{
    struct stream *stream = argument;
    if (!size || !count) return 0;
    size_t available = (stream->length - stream->position) / size;
    if (count > available) count = available;
    size_t got = fread(output, size, count, stream->file);
    stream->position += got * size; return got;
}
static int stream_seek(void *argument, ogg_int64_t offset, int origin)
{
    struct stream *stream = argument;
    int64_t base = origin == SEEK_SET ? 0 : origin == SEEK_CUR ? (int64_t)stream->position :
        origin == SEEK_END ? (int64_t)stream->length : -1;
    if (base < 0 || offset < -base || offset > (int64_t)stream->length - base) return -1;
    int64_t next = base + offset;
    if (fseeko(stream->file, (off_t)stream->offset + next, SEEK_SET)) return -1;
    stream->position = next; return 0;
}
static long stream_tell(void *argument) { return ((struct stream *)argument)->position; }
static void sleep_ms(unsigned milliseconds)
{
    struct timespec delay = {milliseconds / 1000, (long)(milliseconds % 1000) * 1000000};
    nanosleep(&delay, NULL);
}
static int controls(ons_audio *audio, int *paused, int *volume)
{
    pthread_mutex_lock(&audio->lock);
    int stop = audio->stop; *paused = audio->paused; *volume = audio->volume;
    pthread_mutex_unlock(&audio->lock); return stop;
}
static int pcm_frames(ons_audio *audio, snd_pcm_t *pcm, int16_t *samples, unsigned frames, unsigned channels)
{
    unsigned written = 0; uint64_t last_progress = ons_monotonic_ms();
    while (written < frames) {
        int paused, volume;
        if (controls(audio, &paused, &volume)) return 1;
        if (paused) { sleep_ms(10); last_progress = ons_monotonic_ms(); continue; }
        snd_pcm_sframes_t result = snd_pcm_writei(pcm, samples + written * channels, frames - written);
        if (result > 0) {
            written += result; last_progress = ons_monotonic_ms();
            pthread_mutex_lock(&audio->lock); audio->stats.frames += result; pthread_mutex_unlock(&audio->lock);
        } else if (result == -EPIPE || result == -ESTRPIPE) {
            pthread_mutex_lock(&audio->lock); ++audio->stats.underruns; pthread_mutex_unlock(&audio->lock);
            if (snd_pcm_prepare(pcm) < 0) return -1;
        } else if (result == -EAGAIN || result == -EINTR || result == 0) {
            snd_pcm_wait(pcm, 20);
        } else return -1;
        if (ons_monotonic_ms() - last_progress > 5000) return -1;
    }
    return 0;
}
static void *worker(void *argument)
{
    ons_audio *audio = argument;
    struct stream stream = {0}; OggVorbis_File vorbis; snd_pcm_t *pcm = NULL;
    int vorbis_open = 0, result = -1, section = 0;
    uint64_t begin = ons_monotonic_ms();
    int16_t *samples = malloc(32768);
    if (!samples || ons_nsa_stream(audio->archive, audio->resource, &stream.file, &stream.offset, &stream.length)) goto done;
    ov_callbacks callbacks = {stream_read, stream_seek, NULL, stream_tell};
    if (ov_open_callbacks(&stream, &vorbis, NULL, 0, callbacks)) goto done;
    vorbis_open = 1;
    vorbis_info *info = ov_info(&vorbis, -1);
    if (!info || info->channels < 1 || info->channels > 2 || info->rate < 8000 || info->rate > 96000) goto done;
    unsigned channels = info->channels, rate = info->rate;
    if (snd_pcm_open(&pcm, audio->device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) < 0 ||
        snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, channels, rate, 1, 180000) < 0) goto done;
    pthread_mutex_lock(&audio->lock); audio->stats.rate = rate; audio->stats.channels = channels; pthread_mutex_unlock(&audio->lock);
    unsigned consecutive_holes = 0;
    for (;;) {
        int paused, volume;
        if (controls(audio, &paused, &volume)) { result = 0; break; }
        if (paused) { sleep_ms(10); continue; }
        long bytes = ov_read(&vorbis, (char *)samples, 32768, 0, 2, 1, &section);
        if (bytes == OV_HOLE) {
            pthread_mutex_lock(&audio->lock); ++audio->stats.holes; pthread_mutex_unlock(&audio->lock);
            if (++consecutive_holes > 16) break;
            continue;
        }
        consecutive_holes = 0;
        if (bytes < 0) break;
        if (!bytes) {
            if (audio->loop && !ov_pcm_seek(&vorbis, 0)) continue;
            result = 0;
            uint64_t deadline = ons_monotonic_ms() + 2000;
            int drained;
            while ((drained = snd_pcm_drain(pcm)) == -EAGAIN && ons_monotonic_ms() < deadline) {
                if (controls(audio, &paused, &volume)) break;
                snd_pcm_wait(pcm, 20);
            }
            if (drained < 0 && drained != -EAGAIN) result = -1;
            break;
        }
        info = ov_info(&vorbis, section);
        if (!info || info->channels != (int)channels || info->rate != (long)rate || bytes % (channels * 2)) break;
        for (long i = 0; i < bytes / 2; ++i) samples[i] = (int16_t)((int)samples[i] * volume / 100);
        int rc = pcm_frames(audio, pcm, samples, bytes / (channels * 2), channels);
        if (rc) { result = rc == 1 ? 0 : -1; break; }
    }
done:
    if (pcm) { snd_pcm_drop(pcm); snd_pcm_close(pcm); }
    if (vorbis_open) ov_clear(&vorbis);
    if (stream.file) fclose(stream.file);
    free(samples);
    pthread_mutex_lock(&audio->lock);
    audio->stats.elapsed_ms = ons_monotonic_ms() - begin;
    audio->stats.result = result; audio->stats.running = 0;
    pthread_mutex_unlock(&audio->lock); return NULL;
}
int ons_audio_open(ons_audio *audio)
{
    if (!audio) return -1;
    memset(audio, 0, sizeof(*audio)); audio->joined = 1; audio->volume = 100;
    return pthread_mutex_init(&audio->lock, NULL) ? -1 : 0;
}
void ons_audio_stop(ons_audio *audio)
{
    pthread_mutex_lock(&audio->lock); audio->stop = 1; pthread_mutex_unlock(&audio->lock);
    if (!audio->joined) { pthread_join(audio->worker, NULL); audio->joined = 1; }
}
int ons_audio_start(ons_audio *audio, const char *archive, const char *resource, const char *device, int loop)
{
    if (!audio || !archive || !resource || !device || strlen(archive) >= sizeof(audio->archive) ||
        strlen(resource) >= sizeof(audio->resource) || strlen(device) >= sizeof(audio->device)) return -1;
    ons_audio_stop(audio);
    strcpy(audio->archive, archive); strcpy(audio->resource, resource); strcpy(audio->device, device);
    pthread_mutex_lock(&audio->lock);
    audio->stop = audio->paused = 0; audio->loop = !!loop; memset(&audio->stats, 0, sizeof(audio->stats)); audio->stats.running = 1;
    pthread_mutex_unlock(&audio->lock);
    if (pthread_create(&audio->worker, NULL, worker, audio)) {
        pthread_mutex_lock(&audio->lock); audio->stats.running = 0; audio->stats.result = -1; pthread_mutex_unlock(&audio->lock); return -1;
    }
    audio->joined = 0; return 0;
}
void ons_audio_volume(ons_audio *audio, unsigned percent)
{
    pthread_mutex_lock(&audio->lock); audio->volume = percent > 100 ? 100 : percent; pthread_mutex_unlock(&audio->lock);
}
void ons_audio_pause(ons_audio *audio, int paused)
{
    pthread_mutex_lock(&audio->lock); audio->paused = !!paused; pthread_mutex_unlock(&audio->lock);
}
void ons_audio_snapshot(ons_audio *audio, ons_audio_stats *stats)
{
    pthread_mutex_lock(&audio->lock); *stats = audio->stats; pthread_mutex_unlock(&audio->lock);
}
void ons_audio_close(ons_audio *audio) { ons_audio_stop(audio); pthread_mutex_destroy(&audio->lock); }
