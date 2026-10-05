#define _POSIX_C_SOURCE 200809L
#include "audio.h"
#include "events.h"
#include "native.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

static void wait_ms(unsigned ms)
{
    struct timespec pause = {ms / 1000, (long)(ms % 1000) * 1000000}; nanosleep(&pause, NULL);
}
int main(int argc, char **argv)
{
    if (argc != 4) { fprintf(stderr, "usage: native-audio-tests arc1.nsa resource.ogg device\n"); return 1; }
    FILE *file; uint32_t offset, length;
    assert(!ons_nsa_stream(argv[1], argv[2], &file, &offset, &length));
    unsigned char signature[4]; assert(fread(signature, 1, 4, file) == 4 && !memcmp(signature, "OggS", 4)); fclose(file);
    assert(ons_nsa_stream(argv[1], "missing.ogg", &file, &offset, &length) == -1 && !file);
    ons_audio audio; ons_audio_stats first, second;
    assert(!ons_audio_open(&audio));
    ons_audio_volume(&audio, 150); assert(audio.volume == 100);
    ons_audio_volume(&audio, 0); assert(audio.volume == 0);
    ons_audio_volume(&audio, 50);
    assert(!ons_audio_start(&audio, argv[1], argv[2], argv[3], 1));
    uint64_t deadline = ons_monotonic_ms() + 3000;
    do { wait_ms(10); ons_audio_snapshot(&audio, &first); } while (first.running && !first.frames && ons_monotonic_ms() < deadline);
    assert(first.running && first.frames && first.rate && first.channels);
    ons_audio_pause(&audio, 1); wait_ms(150); ons_audio_snapshot(&audio, &first);
    wait_ms(250); ons_audio_snapshot(&audio, &second);
    assert(second.running && first.frames == second.frames);
    ons_audio_volume(&audio, 25); ons_audio_pause(&audio, 0); wait_ms(400);
    ons_audio_snapshot(&audio, &second); assert(second.running && second.frames > first.frames);
    ons_audio_pause(&audio, 1); wait_ms(30);
    uint64_t stopping = ons_monotonic_ms(); ons_audio_stop(&audio);
    ons_audio_snapshot(&audio, &second);
    assert(!second.running && !second.result && ons_monotonic_ms() - stopping < 2000);
    printf("Pause/resume/volume/stop PASS frames=%llu underruns=%u\n", (unsigned long long)second.frames, second.underruns);
    assert(!ons_audio_start(&audio, argv[1], "missing.ogg", argv[3], 0));
    deadline = ons_monotonic_ms() + 3000;
    do { wait_ms(10); ons_audio_snapshot(&audio, &second); } while (second.running && ons_monotonic_ms() < deadline);
    assert(!second.running && second.result == -1 && !second.frames);
    ons_audio_stop(&audio);
    assert(!ons_audio_start(&audio, argv[1], argv[2], argv[3], 0));
    deadline = ons_monotonic_ms() + 3000;
    do { wait_ms(10); ons_audio_snapshot(&audio, &second); } while (second.running && !second.frames && ons_monotonic_ms() < deadline);
    assert(second.running && second.frames);
    ons_audio_close(&audio);
    puts("Native NSA stream/missing-resource/restart/lifecycle tests PASS (no SDL)"); return 0;
}
