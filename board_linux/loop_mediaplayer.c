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
#include <fdk-aac/aacdecoder_lib.h>
#include "render/mediaplayer.h"
#include <unistd.h>

extern mediaplayer_t g_mediaplayer;
atomic_int wiliwili_first_frame_visible = ATOMIC_VAR_INIT(0);
atomic_llong wiliwili_first_video_pts = ATOMIC_VAR_INIT(-1);
static int64_t audio_packet_pts = -1;
static AAC_DECODER_ERROR wiliwili_aac_fill(HANDLE_AACDECODER decoder, UCHAR *buffers[],
                                          const UINT sizes[], UINT *valid, int64_t pts)
{
    static int logged;
    audio_packet_pts = pts;
    if (!logged) { fprintf(stderr, "PLAYER first audio packet pts_us=%lld\n", (long long)pts); logged = 1; }
    return aacDecoder_Fill(decoder, buffers, sizes, valid);
}

atomic_int wiliwili_crop_ready = ATOMIC_VAR_INIT(0);
int wiliwili_crop_left, wiliwili_crop_width, wiliwili_crop_height;

static VideoPicture *wiliwili_request_picture(VideoDecoder *decoder, int stream)
{
    VideoPicture *picture = RequestPicture(decoder, stream);
    static int logged;
    static int preroll_done;
    static unsigned preroll_frames;
    if (picture && !preroll_done) {
        const char *start = getenv("F1_PLAYER_START_MS");
        int64_t target = start ? (int64_t)strtol(start, NULL, 10) * 1000 : 0;
        if (target > 0 && picture->nPts >= 0 && picture->nPts < target) {
            if (!preroll_frames) fprintf(stderr, "PLAYER seek preroll first_pts_us=%lld target_us=%lld\n",
                                        (long long)picture->nPts, (long long)target);
            ++preroll_frames;
            ReturnPicture(decoder, picture);
            return NULL;
        }
        preroll_done = 1;
        atomic_store(&wiliwili_first_video_pts, picture->nPts);
        fprintf(stderr, "PLAYER first display pts_us=%lld preroll_frames=%u\n",
                (long long)picture->nPts, preroll_frames);
    }
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
    while (!atomic_load(&wiliwili_first_frame_visible)) {
        pthread_rwlock_rdlock(&g_mediaplayer.thread.rwlock);
        int stopped = g_mediaplayer.thread.requested_stop;
        pthread_rwlock_unlock(&g_mediaplayer.thread.rwlock);
        if (stopped) return -EINTR;
        usleep(2000);
    }
    int64_t first_video = atomic_load(&wiliwili_first_video_pts);
    if (audio_packet_pts >= 0 && first_video >= 0 && audio_packet_pts < first_video) {
        static int logged;
        if (!logged) { fprintf(stderr, "PLAYER discard early audio PCM pts_us=%lld video_us=%lld\n",
                              (long long)audio_packet_pts, (long long)first_video); logged = 1; }
        return frames;
    }
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
#define aacDecoder_Fill(dec, buffers, sizes, valid) wiliwili_aac_fill(dec, buffers, sizes, valid, pkt->pts)
#include "render/mediaplayer.c"
