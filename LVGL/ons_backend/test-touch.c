#include <assert.h>
#include <stdio.h>
#include "touch_events.h"
int main(void)
{
    struct ons_touch_state state = {0};
    SDL_Event events[2];
    assert(ons_touch_events(&state, 640, 480, 383, 592, 1, events) == 2);
    assert(events[0].type == SDL_MOUSEMOTION && events[0].motion.state == 0);
    assert(events[0].motion.x == 512 && events[0].motion.y == 96);
    assert(events[1].type == SDL_MOUSEBUTTONDOWN && state.down);
    assert(ons_touch_events(&state, 640, 480, 383, 592, 1, events) == 0);
    assert(ons_touch_events(&state, 640, 480, 383, 592, 0, events) == 2);
    assert(events[0].type == SDL_MOUSEMOTION && events[1].type == SDL_MOUSEBUTTONUP && !state.down);
    assert(ons_touch_events(&state, 640, 480, 388, 750, 1, events) == 0);
    assert(ons_touch_events(&state, 640, 480, 388, 750, 0, events) == 0);
    assert(ons_touch_events(&state, 640, 480, 200, 400, 1, events) == 2);
    assert(ons_touch_events(&state, 640, 480, 210, 410, 1, events) == 1);
    assert(events[0].motion.state == SDL_BUTTON_LMASK);
    assert(ons_touch_events(&state, 640, 480, 200, 10, 0, events) == 2);
    assert(events[0].motion.x == 640 && events[0].motion.y == 480);
    assert(events[1].type == SDL_MOUSEBUTTONUP && !state.captured);
    puts("ONS touch mapping/order/tap/drag/cancel tests PASS");
    return 0;
}
