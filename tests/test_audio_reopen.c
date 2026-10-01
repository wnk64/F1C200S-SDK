#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdatomic.h>
#include <time.h>
#include <unistd.h>
static char route_path[128];
#define AUDIO_ROUTE_STATE route_path
#define AUDIO_RETRY_MS 30
#include "../wfd_audio.c"

struct snd_pcm { int route; };
static atomic_int route, offline, param_error, write_error, wait_error, blocked;
static atomic_int opened, closed, cache_freed, partial, again, recovered;
static atomic_ulong frames_written;

int snd_config_update_free_global(void) { atomic_fetch_add(&cache_freed, 1); return 0; }
int snd_pcm_open(snd_pcm_t **pcm, const char *name, int stream, int flags)
{
    (void)name; (void)stream; (void)flags;
    atomic_fetch_add(&opened, 1);
    if (atomic_load(&offline)) return -ENODEV;
    *pcm = malloc(sizeof(**pcm));
    assert(*pcm);
    (*pcm)->route = atomic_load(&route);
    return 0;
}
int snd_pcm_set_params(snd_pcm_t *pcm, int format, int access, unsigned channels,
                       unsigned rate, int resample, unsigned latency)
{
    (void)pcm; (void)format; (void)access; (void)resample;
    assert(channels == 2 && rate == 48000 && latency == 40000);
    return atomic_exchange(&param_error, 0) ? -EINVAL : 0;
}
int snd_pcm_drop(snd_pcm_t *pcm) { assert(pcm); return 0; }
int snd_pcm_close(snd_pcm_t *pcm) { free(pcm); atomic_fetch_add(&closed, 1); return 0; }
snd_pcm_sframes_t snd_pcm_writei(snd_pcm_t *pcm, const void *data, snd_pcm_uframes_t frames)
{
    (void)data;
    assert(pcm);
    if (atomic_load(&blocked)) return -EAGAIN;
    if (atomic_exchange(&again, 0)) return -EAGAIN;
    int err = atomic_exchange(&write_error, 0);
    if (err) return err;
    if (atomic_exchange(&partial, 0) && frames > 1) frames /= 2;
    atomic_fetch_add(&frames_written, frames);
    return (snd_pcm_sframes_t)frames;
}
int snd_pcm_wait(snd_pcm_t *pcm, int timeout)
{
    (void)pcm; (void)timeout;
    struct timespec delay = {0, 1000000}; nanosleep(&delay, NULL);
    return atomic_exchange(&wait_error, 0);
}
int snd_pcm_recover(snd_pcm_t *pcm, int err, int silent)
{
    (void)pcm; (void)silent;
    if (err == -EPIPE) { atomic_fetch_add(&recovered, 1); return 0; }
    return err;
}
const char *snd_strerror(int error) { (void)error; return "mock device error"; }

static void pause_ms(int ms)
{
    struct timespec delay = {ms / 1000, (ms % 1000) * 1000000};
    nanosleep(&delay, NULL);
}
static void set_route(int value)
{
    char temporary[160]; snprintf(temporary, sizeof(temporary), "%s.new", route_path);
    FILE *f = fopen(temporary, "w"); assert(f);
    fprintf(f, "%s\n", value == 1 ? "bluetooth 12:11:71:41:9C:4A" : "headphone");
    fclose(f); atomic_store(&route, value); assert(!rename(temporary, route_path));
}
static void feed(void)
{
    uint8_t data[1024] = {0}; pcm_arrived(audio, data, sizeof(data), 0);
}
static void wait_writes(unsigned long before)
{
    int n;
    for (n = 0; n < 300 && atomic_load(&frames_written) <= before; n++) {
        feed(); pause_ms(2);
    }
    assert(atomic_load(&frames_written) > before);
}
static void wait_opens(int expected)
{
    for (int n = 0; n < 300 && atomic_load(&opened) < expected; n++) pause_ms(2);
    assert(atomic_load(&opened) >= expected);
}
static unsigned long long stat_written(void)
{
    pthread_mutex_lock(&audio->lock); unsigned long long result = audio->written;
    pthread_mutex_unlock(&audio->lock); return result;
}
int main(void)
{
    snprintf(route_path, sizeof(route_path), "/tmp/f1-audio-route-test-%ld", (long)getpid());
    set_route(1);
    assert(!wfd_audio_start("default"));
    atomic_store(&partial, 1); atomic_store(&again, 1);
    feed(); wait_writes(0); pause_ms(15);
    assert(stat_written() == atomic_load(&frames_written) * 4);
    assert(atomic_load(&opened) == 1);
    unsigned long before = atomic_load(&frames_written);
    atomic_store(&write_error, -EPIPE); wait_writes(before);
    assert(atomic_load(&recovered) == 1);
    before = atomic_load(&frames_written);
    atomic_store(&offline, 1); atomic_store(&write_error, -ENODEV); feed();
    wait_opens(3); assert(atomic_load(&closed) >= 1);
    int attempts = atomic_load(&opened); pause_ms(70);
    assert(atomic_load(&opened) - attempts <= 3);
    atomic_store(&offline, 0); wait_writes(before);
    attempts = atomic_load(&opened); set_route(2); wait_opens(attempts + 1);
    before = atomic_load(&frames_written); wait_writes(before);
    attempts = atomic_load(&opened); set_route(1); wait_opens(attempts + 1);
    before = atomic_load(&frames_written); wait_writes(before);
    pthread_mutex_lock(&audio->lock); assert(audio->route_changes >= 2); pthread_mutex_unlock(&audio->lock);
    atomic_store(&blocked, 1); feed(); pause_ms(5);
    attempts = atomic_load(&opened); set_route(2); wait_opens(attempts + 1);
    attempts = atomic_load(&opened); atomic_store(&wait_error, -ENODEV);
    feed(); wait_opens(attempts + 1);
    int64_t start = audio_now_ms(); wfd_audio_stop(); assert(audio_now_ms() - start < 100);
    atomic_store(&blocked, 0);
    atomic_store(&param_error, 1); assert(!wfd_audio_start("default"));
    before = atomic_load(&frames_written); wait_writes(before); wfd_audio_stop();
    attempts = atomic_load(&opened); assert(!wfd_audio_start("hw:Codec,0"));
    before = atomic_load(&frames_written); wait_writes(before); set_route(2); pause_ms(70);
    assert(atomic_load(&opened) == attempts + 1); wfd_audio_stop();
    atomic_store(&offline, 1); assert(!wfd_audio_start("default")); feed(); pause_ms(5);
    start = audio_now_ms(); wfd_audio_stop(); assert(audio_now_ms() - start < 100);
    atomic_store(&offline, 0);
    assert(!audio); assert(!unlink(route_path));
    puts("audio reopen lifecycle tests passed");
    return 0;
}
