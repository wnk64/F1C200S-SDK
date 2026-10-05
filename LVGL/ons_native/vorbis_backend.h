#ifndef ONS_VORBIS_BACKEND_H
#define ONS_VORBIS_BACKEND_H

#ifdef ONS_USE_TREMOR
#include "ivorbisfile.h"
// Tremor emits native-endian signed 16-bit PCM; the board is little-endian.
static inline long ons_vorbis_read(OggVorbis_File *file, char *output, int bytes,
                                  int endian, int word, int signed_pcm, int *section)
{
    if (endian != 0 || word != 2 || signed_pcm != 1) return OV_EINVAL;
    return ov_read(file, output, bytes, section);
}
#define ov_read ons_vorbis_read
#else
#include <vorbis/vorbisfile.h>
#endif

#endif
