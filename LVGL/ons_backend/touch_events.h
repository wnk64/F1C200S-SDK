#ifndef ONS_TOUCH_EVENTS_H
#define ONS_TOUCH_EVENTS_H
#include <SDL/SDL.h>
#include <string.h>

struct ons_touch_state { int x, y, down, raw_down, captured; };

/* Position must precede button events: ONS selects the last hovered button. */
static int ons_touch_events(struct ons_touch_state *state, int width, int height,
                            int px, int py, int down, SDL_Event events[2])
{
    int x = py - (800 - width) / 2;
    int y = 479 - px - (480 - height) / 2;
    int inside = x >= 0 && x < width && y >= 0 && y < height && !(px >= 388 && py >= 708);
    int edge = down != state->raw_down;
    state->raw_down = down;
    if (!state->captured && !(edge && down && inside)) return 0;
    if (!inside) { x = width; y = height; }
    int moved = x != state->x || y != state->y;
    if (!edge && !moved) return 0;
    memset(events, 0, 2 * sizeof(*events));
    events[0].type = SDL_MOUSEMOTION;
    events[0].motion.x = x; events[0].motion.y = y;
    events[0].motion.xrel = x - state->x;
    events[0].motion.yrel = y - state->y;
    events[0].motion.state = state->down ? SDL_BUTTON_LMASK : 0;
    state->x = x; state->y = y;
    if (!edge) return 1;
    state->captured = state->down = down;
    events[1].type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    events[1].button.button = SDL_BUTTON_LEFT;
    events[1].button.state = down ? SDL_PRESSED : SDL_RELEASED;
    events[1].button.x = x; events[1].button.y = y;
    return 2;
}
#endif
