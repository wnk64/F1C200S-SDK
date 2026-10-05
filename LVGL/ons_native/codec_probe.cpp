extern "C" {
#include "native.h"
}
#include "vorbis_backend.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

struct Input { const uint8_t *data; size_t size, at; };
static size_t read_input(void *out, size_t size, size_t count, void *arg)
{
    Input *input = (Input *)arg;
    if (!size) return 0;
    count = std::min(count, (input->size - input->at) / size);
    std::memcpy(out, input->data + input->at, size * count);
    input->at += size * count;
    return count;
}
static int seek_input(void *arg, ogg_int64_t offset, int origin)
{
    Input *input = (Input *)arg;
    int64_t base = origin == SEEK_SET ? 0 : origin == SEEK_CUR ? input->at :
        origin == SEEK_END ? input->size : -1;
    if (base < 0 || offset < -base || offset > (int64_t)input->size - base) return -1;
    input->at = base + offset;
    return 0;
}
static long tell_input(void *arg) { return ((Input *)arg)->at; }
int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    uint8_t *bytes = NULL; size_t length = 0;
    if (ons_nsa_read(argv[1], argv[2], &bytes, &length)) return 3;
    Input input = {bytes, length, 0}; OggVorbis_File decoder;
    ov_callbacks callbacks = {read_input, seek_input, NULL, tell_input};
    if (ov_open_callbacks(&input, &decoder, NULL, 0, callbacks)) { std::free(bytes); return 4; }
    int result = 0;
    for (int pass = 0; pass < 2; ++pass) {
        if (pass && ov_pcm_seek(&decoder, 0)) { result = 5; break; }
        ogg_int64_t total = ov_pcm_total(&decoder, -1), start = ov_pcm_tell(&decoder);
        uint64_t frames = 0; unsigned holes = 0; int section = 0;
        clock_t begin = clock();
        for (;;) {
            int16_t pcm[2048];
            long got = ov_read(&decoder, (char *)pcm, sizeof(pcm), 0, 2, 1, &section);
            if (got == OV_HOLE && ++holes < 16) continue;
            if (got < 0) { result = 6; break; }
            if (!got) break;
            vorbis_info *info = ov_info(&decoder, section);
            if (!info || info->channels < 1 || info->channels > 2) { result = 7; break; }
            frames += got / (2 * info->channels);
        }
        std::printf("pass=%d declared=%lld start=%lld decoded=%llu end=%lld holes=%u cpu_ms=%.3f\n",
            pass, (long long)total, (long long)start, (unsigned long long)frames,
            (long long)ov_pcm_tell(&decoder), holes, 1000.0 * (clock() - begin) / CLOCKS_PER_SEC);
        if (result) break;
    }
    ov_clear(&decoder); std::free(bytes);
    return result;
}
