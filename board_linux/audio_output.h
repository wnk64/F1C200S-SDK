/* One audio thread owns the PCM. UI/OSD only publish pause state. */
atomic_int wiliwili_paused = ATOMIC_VAR_INIT(0);
static snd_pcm_t *audio_pcm;
static unsigned audio_rate, audio_channels;
static int audio_failed;
static int64_t audio_retry, route_checked;
static char audio_route[80];
static int player_stopping(void)
{
    pthread_rwlock_rdlock(&g_mediaplayer.thread.rwlock);
    int stop = g_mediaplayer.thread.requested_stop;
    pthread_rwlock_unlock(&g_mediaplayer.thread.rwlock);
    return stop;
}
static void audio_release(void)
{
    atomic_store(&audio_clock_active, 0);
    if (audio_pcm) { snd_pcm_drop(audio_pcm); snd_pcm_close(audio_pcm); audio_pcm = NULL; }
}
static int player_pause_wait(void)
{
    while (atomic_load(&wiliwili_paused)) {
        if (player_stopping()) return -1;
        usleep(10000);
    }
    return player_stopping() ? -1 : 0;
}
static void audio_route_check(void)
{
    int64_t now = monotonic_us();
    if (now - route_checked < 250000) return;
    route_checked = now;
    char route[80] = {0};
    FILE *f = fopen("/run/audio-route.state", "r");
    if (f) { if (!fgets(route, sizeof(route), f)) route[0] = 0; fclose(f); }
    if (strcmp(route, audio_route)) {
        snprintf(audio_route, sizeof(audio_route), "%s", route);
        audio_release(); audio_failed = 0; audio_retry = 0;
        fprintf(stderr, "PLAYER audio route changed: %s", route);
    }
}
static int audio_connect(void)
{
    const char *device = audio_failed ? "plughw:Codec,0" : "default";
    snd_config_update_free_global();
    int rc = snd_pcm_open(&audio_pcm, device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if (rc >= 0) rc = snd_pcm_set_params(audio_pcm, SND_PCM_FORMAT_S16_LE,
        SND_PCM_ACCESS_RW_INTERLEAVED, audio_channels, audio_rate, 1, 80000);
    if (rc < 0) { audio_release(); return rc; }
    fprintf(stderr, "PLAYER audio connected: %s rate=%u channels=%u\n", device, audio_rate, audio_channels);
    return 0;
}
static int wiliwili_pcm_open(snd_pcm_t **pcm, const char *name, snd_pcm_stream_t stream, int mode)
{
    (void)name; (void)stream; (void)mode;
    *pcm = (snd_pcm_t *)&audio_pcm;
    return 0;
}
static int wiliwili_pcm_params(snd_pcm_t *pcm, snd_pcm_format_t format,
    snd_pcm_access_t access, unsigned channels, unsigned rate, int resample, unsigned latency)
{
    (void)pcm; (void)format; (void)access; (void)resample; (void)latency;
    audio_release(); audio_channels = channels; audio_rate = rate;
    audio_failed = 0; audio_retry = 0;
    return 0;
}
static int wiliwili_pcm_drop(snd_pcm_t *pcm) { (void)pcm; audio_release(); return 0; }
static int wiliwili_pcm_recover(snd_pcm_t *pcm, int error, int silent)
{ (void)pcm; (void)silent; return error; }
static snd_pcm_sframes_t audio_write(const void *samples, snd_pcm_uframes_t frames)
{
    if (atomic_load(&wiliwili_paused)) {
        audio_release();
        if (player_pause_wait()) return -EINTR;
    }
    audio_route_check();
    if (audio_failed && monotonic_us() >= audio_retry) {
        audio_release(); audio_failed = 0;
    }
    if (!audio_pcm && audio_connect() < 0) {
        audio_failed = 1; audio_retry = monotonic_us() + 3000000;
        if (audio_connect() < 0) {
            atomic_store(&audio_clock_active, 0);
            usleep((uint64_t)frames * 1000000 / audio_rate);
            return frames;
        }
    }
    int64_t deadline = monotonic_us() + 500000;
    for (;;) {
        if (player_stopping()) return -EINTR;
        if (atomic_load(&wiliwili_paused)) {
            audio_release();
            if (player_pause_wait()) return -EINTR;
            return 0;
        }
        snd_pcm_sframes_t rc = snd_pcm_writei(audio_pcm, samples, frames);
        if (rc >= 0) return rc;
        if (rc == -EPIPE && snd_pcm_prepare(audio_pcm) >= 0) continue;
        if (rc == -EAGAIN && monotonic_us() < deadline) { usleep(2000); continue; }
        fprintf(stderr, "PLAYER audio transport failed: %s; fallback/reopen\n", snd_strerror(rc));
        audio_release(); audio_failed = 1; audio_retry = monotonic_us() + 3000000;
        if (audio_connect() >= 0) return 0;
        usleep((uint64_t)frames * 1000000 / audio_rate);
        return frames;
    }
}
