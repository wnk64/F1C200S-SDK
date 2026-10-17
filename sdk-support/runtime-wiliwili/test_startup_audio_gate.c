#include <assert.h>
#include <stdio.h>
#include "startup_audio_gate.h"
int main(void)
{
    assert(!wiliwili_startup_audio_discard(-1, 55000000, -1));
    assert(!wiliwili_startup_audio_discard(0, 0, -1));
    assert(wiliwili_startup_audio_discard(49969433, 55000000, -1));
    assert(!wiliwili_startup_audio_discard(55000000, 55000000, -1));
    assert(wiliwili_startup_audio_discard(55000000, 55000000, 55100000));
    assert(!wiliwili_startup_audio_discard(55100000, 55000000, 55100000));
    /* The old wait consumed just one packet and stalled the parser at128.
     * Five seconds of preroll must drain even with no first frame yet. */
    unsigned drained = 0;
    for (int64_t pts = 49969433; pts < 55000000; pts += 23220) {
        assert(wiliwili_startup_audio_discard(pts, 55000000, -1));
        ++drained;
    }
    assert(drained > 128);
    puts("PASS startup/unknown/exact boundary/first video/5s seek audio preroll");
    return 0;
}
