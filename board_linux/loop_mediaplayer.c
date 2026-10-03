#include <CdxParser.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>

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
#include "render/mediaplayer.c"
