#include "engine_runtime.h"
#include <alsa/asoundlib.h>
#include <vorbis/vorbisfile.h>
#include <pthread.h>
#include <atomic>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <new>

struct MemoryAudio { const uint8_t *bytes; size_t length, position; };
static size_t read_audio(void *output, size_t size, size_t count, void *argument)
{
    MemoryAudio *stream = (MemoryAudio *)argument;
    if (!size || !count) return 0;
    count = std::min(count, (stream->length - stream->position) / size);
    if (!count) return 0;
    std::memcpy(output, stream->bytes + stream->position, count * size); stream->position += count * size; return count;
}
static int seek_audio(void *argument, ogg_int64_t offset, int origin)
{
    MemoryAudio *stream = (MemoryAudio *)argument;
    int64_t base = origin == SEEK_SET ? 0 : origin == SEEK_CUR ? (int64_t)stream->position :
        origin == SEEK_END ? (int64_t)stream->length : -1;
    if (base < 0 || offset < -base || offset > (int64_t)stream->length - base) return -1;
    stream->position = base + offset; return 0;
}
static long tell_audio(void *argument) { return ((MemoryAudio *)argument)->position; }
static const ov_callbacks callbacks = {read_audio, seek_audio, NULL, tell_audio};
struct ons_mix_music {
    std::vector<uint8_t> bytes;
    MemoryAudio input;
    OggVorbis_File decoder;
    bool opened;
    ons_mix_music() : input{}, opened(false) {}
    ~ons_mix_music() { if (opened) ov_clear(&decoder); }
};
struct Channel { ons_mix_Chunk *chunk; size_t position; int volume, loops; bool active, paused; };
static pthread_mutex_t mixer_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t mixer_thread;
static std::atomic<bool> stopping(false);
static bool opened;
static snd_pcm_t *device;
static Channel channels[64];
static int allocated = 54, rate = 44100, music_volume = 128, music_loops;
static ons_mix_Music *music;
static void (*music_hook)(void *, uint8_t *, int);
static void *hook_argument;
static void (*music_finished)(void);
static void (*channel_finished)(int);
static unsigned underruns;
static uint64_t written;
static int16_t last_sample[2];
static thread_local char mixer_error[128];
static void set_error(const char *message) { std::snprintf(mixer_error, sizeof(mixer_error), "%s", message); }
const char *ons_mix_GetError(void) { return mixer_error; }
static void pause_ms(unsigned ms) { timespec pause = {(time_t)(ms / 1000), (long)(ms % 1000) * 1000000}; nanosleep(&pause, NULL); }
static bool ogg_format(OggVorbis_File *decoder)
{
    vorbis_info *info = ov_info(decoder, -1);
    return info && info->rate == rate && (info->channels == 1 || info->channels == 2);
}
static int read_music(ons_mix_Music *track, int16_t *output, unsigned frames)
{
    unsigned filled = 0, holes = 0, empty_rewinds = 0; int section = 0;
    while (filled < frames) {
        vorbis_info *info = ov_info(&track->decoder, -1);
        if (!info || info->rate != rate || info->channels < 1 || info->channels > 2) return -1;
        int16_t temporary[2048];
        unsigned wanted = frames - filled;
        long bytes = ov_read(&track->decoder, (char *)temporary, wanted * info->channels * 2, 0, 2, 1, &section);
        if (bytes == OV_HOLE && ++holes < 16) continue;
        if (bytes < 0) return -1;
        if (!bytes) {
            if (music_loops && ++empty_rewinds <= 2 && !ov_pcm_seek(&track->decoder, 0)) { if (music_loops > 0) --music_loops; continue; }
            break;
        }
        unsigned got = bytes / (info->channels * 2);
        if (!got) return -1;
        empty_rewinds = 0;
        for (unsigned i = 0; i < got; ++i) {
            output[(filled + i) * 2] = temporary[i * info->channels];
            output[(filled + i) * 2 + 1] = temporary[i * info->channels + info->channels - 1];
        }
        filled += got;
    }
    return filled;
}
static void render(int16_t output[2048], int ended[64], unsigned *ended_count, bool *music_ended)
{
    int32_t sum[2048] = {}; int16_t music_pcm[2048] = {};
    *ended_count = 0; *music_ended = false;
    pthread_mutex_lock(&mixer_lock);
    if (music_hook) music_hook(hook_argument, (uint8_t *)music_pcm, sizeof(music_pcm));
    else if (music) {
        int got = read_music(music, music_pcm, 1024);
        if (got < 1024) { music = NULL; *music_ended = true; }
    }
    for (unsigned i = 0; i < 2048; ++i) sum[i] = (int)music_pcm[i] * music_volume / 128;
    for (int channel = 0; channel < allocated; ++channel) {
        Channel &state = channels[channel];
        if (!state.active || state.paused || !state.chunk) continue;
        unsigned frame = 0;
        while (frame < 1024 && state.active) {
            if (state.position >= state.chunk->alen) {
                if (state.loops) { state.position = 0; if (state.loops > 0) --state.loops; }
                else { state.active = false; ended[(*ended_count)++] = channel; break; }
            }
            unsigned available = (state.chunk->alen - state.position) / 4;
            unsigned count = std::min(1024 - frame, available);
            if (!count) { state.active = false; ended[(*ended_count)++] = channel; break; }
            const int16_t *input = (const int16_t *)(state.chunk->abuf + state.position);
            int volume = state.volume * state.chunk->volume;
            for (unsigned i = 0; i < count * 2; ++i) sum[frame * 2 + i] += (int)input[i] * volume / (128 * 128);
            state.position += count * 4; frame += count;
        }
    }
    for (unsigned i = 0; i < 2048; ++i) output[i] = std::max(-32768, std::min(32767, sum[i]));
    last_sample[0] = output[0]; last_sample[1] = output[1];
    pthread_mutex_unlock(&mixer_lock);
}
static void *mix_loop(void *argument)
{
    bool null_device = argument != NULL;
    while (!stopping) {
        int16_t samples[2048]; int ended[64]; unsigned ended_count; bool music_ended;
        render(samples, ended, &ended_count, &music_ended);
        pthread_mutex_lock(&mixer_lock); auto channel_callback = channel_finished; auto music_callback = music_finished; pthread_mutex_unlock(&mixer_lock);
        for (unsigned i = 0; i < ended_count; ++i) if (channel_callback) channel_callback(ended[i]);
        if (music_ended && music_callback) music_callback();
        unsigned sent = 0, stalls = 0;
        while (sent < 1024 && !stopping) {
            snd_pcm_sframes_t result = snd_pcm_writei(device, samples + sent * 2, 1024 - sent);
            if (result > 0) { sent += result; stalls = 0; pthread_mutex_lock(&mixer_lock); written += result; pthread_mutex_unlock(&mixer_lock); }
            else if (result == -EPIPE || result == -ESTRPIPE) {
                pthread_mutex_lock(&mixer_lock); ++underruns; pthread_mutex_unlock(&mixer_lock);
                if (snd_pcm_prepare(device) < 0) { stopping = true; break; }
            } else if (result == -EAGAIN || result == -EINTR || !result) {
                if (++stalls > 250) { stopping = true; break; }
                snd_pcm_wait(device, 20);
            } else { stopping = true; break; }
        }
        if (null_device) pause_ms(23);
    }
    return NULL;
}
int ons_mix_OpenAudio(int frequency, uint16_t format, int channel_count, int)
{
    if (frequency != 44100 || format != ons_mix_DEFAULT_FORMAT || channel_count != 2) { set_error("Native mixer requires 44100Hz S16LE stereo output"); return -1; }
    if (opened) return 0;
    const char *name = std::getenv("ONS_NATIVE_AUDIO_DEVICE"); if (!name) name = "default";
    if (snd_pcm_open(&device, name, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) < 0) { set_error("ALSA open failed"); return -1; }
    if (snd_pcm_set_params(device, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 2, frequency, 1, 350000) < 0) {
        snd_pcm_close(device); device = NULL; set_error("ALSA parameters failed"); return -1;
    }
    rate = frequency; stopping = false; underruns = 0; written = 0;
    for (int i = 0; i < 64; ++i) channels[i].volume = 128;
    if (pthread_create(&mixer_thread, NULL, mix_loop, !std::strcmp(name, "null") ? (void *)1 : NULL)) {
        snd_pcm_close(device); device = NULL; set_error("Mixer worker creation failed"); return -1;
    }
    opened = true; std::atexit(ons_mix_CloseAudio); return 0;
}
void ons_mix_CloseAudio(void)
{
    if (!opened) return;
    stopping = true; pthread_join(mixer_thread, NULL);
    snd_pcm_drop(device); snd_pcm_close(device); device = NULL;
    pthread_mutex_lock(&mixer_lock);
    for (unsigned i = 0; i < 64; ++i) channels[i] = {};
    music = NULL; music_hook = NULL; hook_argument = NULL; opened = false;
    pthread_mutex_unlock(&mixer_lock);
}
int ons_mix_QuerySpec(int *frequency, uint16_t *format, int *channel_count)
{
    if (!opened) return 0;
    if (frequency) *frequency = rate;
    if (format) *format = ons_mix_DEFAULT_FORMAT;
    if (channel_count) *channel_count = 2;
    return 1;
}
int ons_mix_AllocateChannels(int number)
{
    pthread_mutex_lock(&mixer_lock);
    if (number >= 0) {
        number = std::min(64, number);
        for (int i = number; i < allocated; ++i) channels[i] = {};
        for (int i = allocated; i < number; ++i) channels[i].volume = 128;
        allocated = number;
    }
    int result = allocated; pthread_mutex_unlock(&mixer_lock); return result;
}
void ons_mix_ChannelFinished(void (*callback)(int)) { pthread_mutex_lock(&mixer_lock); channel_finished = callback; pthread_mutex_unlock(&mixer_lock); }
void ons_mix_HookMusicFinished(void (*callback)(void)) { pthread_mutex_lock(&mixer_lock); music_finished = callback; pthread_mutex_unlock(&mixer_lock); }
void ons_mix_HookMusic(void (*callback)(void *, uint8_t *, int), void *argument)
{
    pthread_mutex_lock(&mixer_lock); music_hook = callback; hook_argument = argument; music = NULL; pthread_mutex_unlock(&mixer_lock);
}
ons_mix_Chunk *ons_mix_QuickLoad_RAW(uint8_t *bytes, uint32_t length)
{
    if (!bytes || !length || length % 4 || length > 8388608) return NULL;
    ons_mix_Chunk *chunk = (ons_mix_Chunk *)std::calloc(1, sizeof(*chunk));
    if (chunk) { chunk->abuf = bytes; chunk->alen = length; chunk->volume = 128; }
    return chunk;
}
void ons_mix_FreeChunk(ons_mix_Chunk *chunk)
{
    if (!chunk) return;
    pthread_mutex_lock(&mixer_lock);
    for (unsigned i = 0; i < 64; ++i) if (channels[i].chunk == chunk) { channels[i].chunk = NULL; channels[i].active = false; }
    if (chunk->allocated) std::free(chunk->abuf);
    std::free(chunk); pthread_mutex_unlock(&mixer_lock);
}
int ons_mix_PlayChannel(int channel, ons_mix_Chunk *chunk, int loops)
{
    if (!opened || !chunk || !chunk->abuf || !chunk->alen || chunk->alen % 4) return -1;
    pthread_mutex_lock(&mixer_lock);
    if (channel == -1) { for (channel = 0; channel < allocated && channels[channel].active; ++channel) {} }
    if (channel < 0 || channel >= allocated) { pthread_mutex_unlock(&mixer_lock); return -1; }
    Channel &state = channels[channel]; state.chunk = chunk; state.position = 0; state.loops = loops;
    state.active = true; state.paused = false;
    pthread_mutex_unlock(&mixer_lock); return channel;
}
void ons_mix_Pause(int channel)
{
    pthread_mutex_lock(&mixer_lock);
    for (int i = 0; i < allocated; ++i) if (channel == -1 || channel == i) channels[i].paused = true;
    pthread_mutex_unlock(&mixer_lock);
}
int ons_mix_Volume(int channel, int volume)
{
    pthread_mutex_lock(&mixer_lock); int result = 0, count = 0;
    for (int i = 0; i < allocated; ++i) if (channel == -1 || channel == i) {
        result += channels[i].volume; ++count; if (volume >= 0) channels[i].volume = std::min(128, volume);
    }
    pthread_mutex_unlock(&mixer_lock); return count ? result / count : -1;
}
int ons_mix_VolumeMusic(int volume)
{
    pthread_mutex_lock(&mixer_lock); int previous = music_volume; if (volume >= 0) music_volume = std::min(128, volume);
    pthread_mutex_unlock(&mixer_lock); return previous;
}
int ons_mix_SetMusicCMD(const char *command)
{
    if (command && *command) { set_error("External MIDI command execution is not supported"); return -1; }
    return 0;
}
ons_mix_Music *ons_mix_LoadMUS_RW(ons_rt_RWops *stream)
{
    if (!stream) return NULL;
    ons_mix_Music *track = new(std::nothrow) ons_mix_Music();
    if (!track || stream->position > stream->length || stream->length - stream->position > 8388608) { delete track; ons_rt_RWclose(stream); return NULL; }
    try { track->bytes.assign(stream->memory + stream->position, stream->memory + stream->length); }
    catch (...) { delete track; ons_rt_RWclose(stream); return NULL; }
    ons_rt_RWclose(stream); track->input = {track->bytes.data(), track->bytes.size(), 0};
    if (ov_open_callbacks(&track->input, &track->decoder, NULL, 0, callbacks)) { delete track; set_error("Native music reader only supports OGG/Vorbis"); return NULL; }
    track->opened = true;
    if (!ogg_format(&track->decoder)) { delete track; set_error("Music sample rate must match 44100Hz mixer"); return NULL; }
    return track;
}
ons_mix_Music *ons_mix_LoadMUS(const char *path)
{
    if (!path) return NULL;
    std::FILE *file = std::fopen(path, "rb"); if (!file) return NULL;
    if (std::fseek(file, 0, SEEK_END)) { std::fclose(file); return NULL; }
    long size = std::ftell(file);
    if (size < 1 || size > 8388608 || std::fseek(file, 0, SEEK_SET)) { std::fclose(file); return NULL; }
    std::vector<uint8_t> bytes;
    try { bytes.resize(size); } catch (...) { std::fclose(file); return NULL; }
    bool ok = std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size(); std::fclose(file);
    return ok ? ons_mix_LoadMUS_RW(ons_rt_RWFromMem(bytes.data(), bytes.size())) : NULL;
}
int ons_mix_PlayMusic(ons_mix_Music *track, int loops)
{
    if (!opened || !track || !track->opened) return -1;
    pthread_mutex_lock(&mixer_lock); int result = ov_pcm_seek(&track->decoder, 0);
    if (!result) { music_hook = NULL; hook_argument = NULL; music = track; music_loops = loops; }
    pthread_mutex_unlock(&mixer_lock); return result ? -1 : 0;
}
int ons_mix_PlayingMusic(void) { pthread_mutex_lock(&mixer_lock); int playing = music || music_hook; pthread_mutex_unlock(&mixer_lock); return playing; }
int ons_mix_SetMusicPosition(double seconds)
{
    if (seconds < 0) return -1;
    pthread_mutex_lock(&mixer_lock); int result = music ? ov_time_seek(&music->decoder, seconds) : -1;
    pthread_mutex_unlock(&mixer_lock); return result;
}
int ons_mix_HaltMusic(void) { pthread_mutex_lock(&mixer_lock); music = NULL; music_hook = NULL; hook_argument = NULL; pthread_mutex_unlock(&mixer_lock); return 0; }
void ons_mix_FreeMusic(ons_mix_Music *track)
{
    if (!track) return;
    pthread_mutex_lock(&mixer_lock); if (music == track) music = NULL; delete track; pthread_mutex_unlock(&mixer_lock);
}
static uint16_t little16(const uint8_t *p) { return p[0] | (uint16_t)p[1] << 8; }
static uint32_t little32(const uint8_t *p) { return little16(p) | (uint32_t)little16(p + 2) << 16; }
ons_mix_Chunk *ons_mix_LoadWAV_RW(ons_rt_RWops *stream, int free_stream)
{
    if (!stream) return NULL;
    if (stream->position > stream->length) { if (free_stream) ons_rt_RWclose(stream); return NULL; }
    std::vector<uint8_t> pcm;
    const uint8_t *bytes = stream->memory + stream->position; size_t length = stream->length - stream->position;
    bool valid = false;
    if (length >= 4 && !std::memcmp(bytes, "OggS", 4)) {
        MemoryAudio input = {bytes, length, 0}; OggVorbis_File decoder;
        if (!ov_open_callbacks(&input, &decoder, NULL, 0, callbacks)) {
            valid = ogg_format(&decoder); int section = 0; unsigned holes = 0;
            while (valid) {
                vorbis_info *info = ov_info(&decoder, -1); int16_t buffer[2048];
                if (!info || info->rate != rate || info->channels < 1 || info->channels > 2) { valid = false; break; }
                long got = ov_read(&decoder, (char *)buffer, sizeof(buffer), 0, 2, 1, &section);
                if (got == OV_HOLE && ++holes < 16) continue;
                if (got < 0) { valid = false; break; }
                if (!got) break;
                unsigned frames = got / (info->channels * 2);
                if (pcm.size() + frames * 4 > 8388608) { valid = false; break; }
                try {
                    size_t before = pcm.size(); pcm.resize(before + frames * 4); int16_t *output = (int16_t *)(pcm.data() + before);
                    for (unsigned i = 0; i < frames; ++i) { output[i * 2] = buffer[i * info->channels]; output[i * 2 + 1] = buffer[i * info->channels + info->channels - 1]; }
                } catch (...) { valid = false; break; }
            }
            ov_clear(&decoder);
        }
    } else if (length >= 12 && !std::memcmp(bytes, "RIFF", 4) && !std::memcmp(bytes + 8, "WAVE", 4)) {
        unsigned channel_count = 0, depth = 0, frequency = 0; const uint8_t *samples = NULL; size_t size = 0;
        for (size_t at = 12; at + 8 <= length;) {
            uint32_t chunk = little32(bytes + at + 4); if (chunk > length - at - 8) break;
            if (!std::memcmp(bytes + at, "fmt ", 4) && chunk >= 16 && little16(bytes + at + 8) == 1) {
                channel_count = little16(bytes + at + 10); frequency = little32(bytes + at + 12); depth = little16(bytes + at + 22);
            } else if (!std::memcmp(bytes + at, "data", 4)) { samples = bytes + at + 8; size = chunk; }
            at += 8 + (uint64_t)chunk + (chunk & 1);
        }
        if (samples && frequency == (unsigned)rate && (channel_count == 1 || channel_count == 2) && (depth == 8 || depth == 16)) {
            unsigned stride = channel_count * (depth / 8); size_t frames = size / stride;
            if (frames && size % stride == 0 && frames <= 2097152) {
                try { pcm.resize(frames * 4); valid = true; } catch (...) {}
                if (valid) for (size_t i = 0; i < frames; ++i) for (unsigned c = 0; c < 2; ++c) {
                    const uint8_t *p = samples + i * stride + (channel_count == 1 ? 0 : c) * (depth / 8);
                    ((int16_t *)pcm.data())[i * 2 + c] = depth == 8 ? ((int)*p - 128) * 256 : (int16_t)little16(p);
                }
            }
        }
    }
    if (free_stream) ons_rt_RWclose(stream);
    if (!valid || pcm.empty()) { set_error("Unsupported or invalid audio chunk/rate"); return NULL; }
    uint8_t *copy = (uint8_t *)std::malloc(pcm.size()); if (!copy) return NULL;
    std::memcpy(copy, pcm.data(), pcm.size()); ons_mix_Chunk *chunk = ons_mix_QuickLoad_RAW(copy, pcm.size());
    if (!chunk) { std::free(copy); return NULL; }
    chunk->allocated = 1; return chunk;
}
void ons_mix_Diagnostics(uint64_t *frames, unsigned *xrun)
{
    pthread_mutex_lock(&mixer_lock); if (frames) *frames = written; if (xrun) *xrun = underruns; pthread_mutex_unlock(&mixer_lock);
}
void ons_mix_LastSample(int16_t output[2])
{
    pthread_mutex_lock(&mixer_lock); output[0] = last_sample[0]; output[1] = last_sample[1]; pthread_mutex_unlock(&mixer_lock);
}
