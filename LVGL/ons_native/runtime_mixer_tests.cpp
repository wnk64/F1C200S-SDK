#include "engine_runtime.h"
#include <cassert>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
static std::atomic<unsigned> finished(0), music_end(0);
static void channel_done(int) { ++finished; }
static void music_done() { ++music_end; }
static void hook(void *, uint8_t *bytes, int size)
{
    int16_t *samples = (int16_t *)bytes; for (int i = 0; i < size / 2; ++i) samples[i] = 20000;
}
static void wait(unsigned ms) { timespec delay = {0, (long)ms * 1000000}; nanosleep(&delay, NULL); }
int main(int argc, char **argv)
{
    setenv("ONS_NATIVE_AUDIO_DEVICE", "null", 1);
    assert(ons_mix_OpenAudio(22050, ons_mix_DEFAULT_FORMAT, 2, 1024) == -1);
    assert(!ons_mix_OpenAudio(44100, ons_mix_DEFAULT_FORMAT, 2, 1024));
    assert(ons_mix_AllocateChannels(100) == 64);
    assert(ons_mix_Volume(3, -1) == 128);
    ons_mix_ChannelFinished(channel_done); ons_mix_HookMusicFinished(music_done);
    uint8_t *bytes = (uint8_t *)std::malloc(8192); assert(bytes);
    for (unsigned i = 0; i < 4096; ++i) ((int16_t *)bytes)[i] = 20000;
    ons_mix_Chunk *chunk = ons_mix_QuickLoad_RAW(bytes, 8192); assert(chunk); chunk->allocated = 1;
    ons_mix_HookMusic(hook, NULL); assert(ons_mix_PlayChannel(3, chunk, -1) == 3);
    wait(80); int16_t observed[2]; ons_mix_LastSample(observed);
    assert(observed[0] == 32767 && observed[1] == 32767);
    ons_mix_Volume(3, 64); wait(80); ons_mix_LastSample(observed); assert(observed[0] == 30000);
    ons_mix_Pause(3); wait(80); ons_mix_LastSample(observed); assert(observed[0] == 20000);
    ons_mix_HookMusic(NULL, NULL); assert(ons_mix_PlayChannel(3, chunk, 0) == 3);
    wait(180); assert(finished.load() == 1);
    assert(ons_mix_PlayChannel(64, chunk, 0) == -1);
    ons_mix_FreeChunk(chunk);
    assert(ons_mix_SetMusicCMD("external-command") == -1);
    if (argc == 2) {
        uint8_t *ogg; size_t length; assert(!ons_nsa_read(argv[1], "sound/sayavoifirstedition10093.ogg", &ogg, &length));
        ons_mix_Music *track = ons_mix_LoadMUS_RW(ons_rt_RWFromMem(ogg, length)); assert(track);
        assert(!ons_mix_PlayMusic(track, 0)); wait(350); assert(!ons_mix_PlayingMusic() && music_end.load() == 1);
        ons_mix_FreeMusic(track);
        chunk = ons_mix_LoadWAV_RW(ons_rt_RWFromMem(ogg, length), 1); assert(chunk && chunk->alen == 7056 * 4);
        assert(ons_mix_PlayChannel(0, chunk, 0) == 0); wait(350); assert(finished.load() == 2);
        ons_mix_FreeChunk(chunk); std::free(ogg);
    }
    uint64_t frames; unsigned xruns; ons_mix_Diagnostics(&frames, &xruns); assert(frames && !xruns);
    ons_mix_CloseAudio(); assert(!ons_mix_QuerySpec(NULL, NULL, NULL));
    std::printf("Native mixer clipping/volume/pause/finish/OGG/free/close PASS frames=%llu xruns=%u\n", (unsigned long long)frames, xruns);
    return 0;
}
