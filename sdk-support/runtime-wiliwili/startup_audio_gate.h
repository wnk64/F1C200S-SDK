#ifndef WILIWILI_STARTUP_AUDIO_GATE_H
#define WILIWILI_STARTUP_AUDIO_GATE_H
#include <stdint.h>
static inline int wiliwili_startup_audio_discard(int64_t pts, int64_t target,
                                                int64_t first_video)
{
    int64_t cutoff = first_video > target ? first_video : target;
    return pts >= 0 && cutoff > 0 && pts < cutoff;
}
#endif
