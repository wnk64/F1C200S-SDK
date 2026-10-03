#include <CdxParser.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdatomic.h>
#include <alsa/asoundlib.h>
#include <string.h>
#include <errno.h>
#include <vdecoder.h>

atomic_int wiliwili_crop_ready = ATOMIC_VAR_INIT(0);
int wiliwili_crop_left, wiliwili_crop_width, wiliwili_crop_height;

static VideoPicture *wiliwili_request_picture(VideoDecoder *decoder, int stream)
{
    VideoPicture *picture = RequestPicture(decoder, stream);
    static int logged;
    if (picture && !logged) {
        logged = 1;
        fprintf(stderr, "PLAYER frame bounds: buffer=%dx%d stride=%d visible=L%d T%d R%d B%d\n",
                picture->nWidth, picture->nHeight, picture->nLineStride,
                picture->nLeftOffset, picture->nTopOffset, picture->nRightOffset, picture->nBottomOffset);
        if (picture->nTopOffset == 0 && picture->nLeftOffset >= 0 && picture->nLeftOffset < 32 &&
            picture->nRightOffset > picture->nLeftOffset && picture->nRightOffset <= picture->nWidth &&
            picture->nBottomOffset > 0 && picture->nBottomOffset <= picture->nHeight) {
            wiliwili_crop_left = picture->nLeftOffset;
            wiliwili_crop_width = picture->nRightOffset - picture->nLeftOffset;
            wiliwili_crop_height = picture->nBottomOffset;
            atomic_store(&wiliwili_crop_ready, 1);
        }
    }
    return picture;
}

atomic_int wiliwili_volume = ATOMIC_VAR_INIT(100);
static int initial_seek_applied;
static cdx_int32 wiliwili_prefetch(CdxParserT *parser, CdxPacketT *packet)
{
    if (!initial_seek_applied) {
        initial_seek_applied = 1;
        const char *value = getenv("F1_PLAYER_START_MS");
        if (value && *value) {
            char *end;
            long ms = strtol(value, &end, 10);
            if (*end || ms < 0 || ms > 86400000) return -1;
            if (ms > 0) {
                int result = CdxParserSeekTo(parser, (int64_t)ms * 1000, AW_SEEK_CLOSEST);
                fprintf(stderr, "PLAYER initial_seek_ms=%ld rc=%d\n", ms, result);
                if (result < 0) return result;
            }
        }
    }
    return CdxParserPrefetch(parser, packet);
}

static snd_pcm_sframes_t wiliwili_pcm_write(snd_pcm_t *pcm, const void *samples, snd_pcm_uframes_t frames)
{
    int volume = atomic_load(&wiliwili_volume);
    if (volume >= 100) return snd_pcm_writei(pcm, samples, frames);
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    unsigned channels;
    if (snd_pcm_hw_params_current(pcm, params) || snd_pcm_hw_params_get_channels(params, &channels)) return -EINVAL;
    static int16_t scaled[8192];
    if (!channels || frames > 8192 / channels) return -EINVAL;
    const int16_t *input = samples;
    for (unsigned i = 0; i < frames * channels; ++i) scaled[i] = (int32_t)input[i] * volume / 100;
    return snd_pcm_writei(pcm, scaled, frames);
}

static cdx_int32 wiliwili_seek_to(CdxParserT *parser, cdx_int64 time_us, SeekModeType mode)
{
    cdx_int32 result = CdxParserSeekTo(parser, time_us, mode);
    if (result == 0 && time_us == 0 && getenv("F1_OSD_EVENT_FD")) {
        const uint64_t notification = 4;
        ssize_t sent = send(3, &notification, sizeof(notification), MSG_DONTWAIT | MSG_NOSIGNAL);
        fprintf(stderr, "PLAYER loop-to-zero: seek_rc=%d UI_event_bytes=%ld\n", result, (long)sent);
    }
    return result;
}

/* Compile the original player without editing its protected source tree. */
#define CdxParserSeekTo wiliwili_seek_to
#define CdxParserPrefetch wiliwili_prefetch
#define snd_pcm_writei wiliwili_pcm_write
#define RequestPicture wiliwili_request_picture
#include "render/mediaplayer.c"
