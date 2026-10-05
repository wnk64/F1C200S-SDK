#ifndef ONS_VORBIS_BACKEND_H
#define ONS_VORBIS_BACKEND_H

#ifdef ONS_USE_TREMOR
#include "ivorbisfile.h"
#include <math.h>
// Tremor emits native-endian signed 16-bit PCM; the board is little-endian.
static inline long ons_vorbis_read(OggVorbis_File *file, char *output, int bytes,
                                  int endian, int word, int signed_pcm, int *section)
{
    if (endian != 0 || word != 2 || signed_pcm != 1) return OV_EINVAL;
    return ov_read(file, output, bytes, section);
}
#define ov_read ons_vorbis_read
static inline int ons_vorbis_time_seek(OggVorbis_File *file, double seconds)
{
    if (!isfinite(seconds) || seconds < 0 || seconds > 9.0e15) return OV_EINVAL;
    return ov_time_seek(file, (ogg_int64_t)(seconds * 1000));
}
#define ov_time_seek ons_vorbis_time_seek
#else
#include <vorbis/vorbisfile.h>
#endif

#endif
