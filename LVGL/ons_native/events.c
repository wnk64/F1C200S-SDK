#define _POSIX_C_SOURCE 200809L
#include "events.h"
#include <string.h>
#include <time.h>
#include <errno.h>

uint64_t ons_monotonic_ms(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return 0;
    return (uint64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
int ons_events_open(ons_events *events)
{
    if (!events) return -1;
    memset(events, 0, sizeof(*events));
    if (pthread_mutex_init(&events->lock, NULL)) return -1;
    pthread_condattr_t attr;
    if (pthread_condattr_init(&attr)) { pthread_mutex_destroy(&events->lock); return -1; }
    int rc = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    if (!rc) rc = pthread_cond_init(&events->changed, &attr);
    pthread_condattr_destroy(&attr);
    if (rc) { pthread_mutex_destroy(&events->lock); return -1; }
    return 0;
}
void ons_events_close(ons_events *events)
{
    pthread_mutex_lock(&events->lock); events->closed = 1;
    pthread_cond_broadcast(&events->changed); pthread_mutex_unlock(&events->lock);
}
/* Owners must join all producers and consumers before destroying the queue. */
void ons_events_destroy(ons_events *events)
{
    pthread_cond_destroy(&events->changed); pthread_mutex_destroy(&events->lock);
}
int ons_events_push(ons_events *events, ons_event event)
{
    pthread_mutex_lock(&events->lock);
    if (events->closed || events->count == 64) { pthread_mutex_unlock(&events->lock); return -1; }
    event.timestamp = ons_monotonic_ms();
    events->pending[(events->head + events->count++) % 64] = event;
    pthread_cond_signal(&events->changed); pthread_mutex_unlock(&events->lock); return 0;
}
int ons_timer_start(ons_events *events, int code, unsigned delay, unsigned period)
{
    pthread_mutex_lock(&events->lock);
    int slot = -1;
    for (unsigned i = 0; i < 16; ++i) {
        if (events->timers[i].active && events->timers[i].code == code) { slot = i; break; }
        if (!events->timers[i].active && slot < 0) slot = i;
    }
    if (slot < 0 || events->closed) { pthread_mutex_unlock(&events->lock); return -1; }
    ons_timer *timer = &events->timers[slot];
    timer->code = code; timer->due = ons_monotonic_ms() + delay; timer->period = period; timer->active = 1;
    pthread_cond_signal(&events->changed); pthread_mutex_unlock(&events->lock); return 0;
}
void ons_timer_cancel(ons_events *events, int code)
{
    pthread_mutex_lock(&events->lock);
    for (unsigned i = 0; i < 16; ++i) if (events->timers[i].code == code) events->timers[i].active = 0;
    pthread_cond_signal(&events->changed); pthread_mutex_unlock(&events->lock);
}
int ons_events_wait(ons_events *events, ons_event *event, unsigned timeout)
{
    if (!events || !event) return -1;
    uint64_t deadline = ons_monotonic_ms() + timeout;
    pthread_mutex_lock(&events->lock);
    for (;;) {
        if (events->closed) { pthread_mutex_unlock(&events->lock); return 0; }
        if (events->count) {
            *event = events->pending[events->head]; events->head = (events->head + 1) % 64; --events->count;
            pthread_mutex_unlock(&events->lock); return 1;
        }
        uint64_t now = ons_monotonic_ms(), wake = deadline;
        int selected = -1;
        for (unsigned i = 0; i < 16; ++i) {
            ons_timer *timer = &events->timers[i];
            if (!timer->active) continue;
            if (timer->due <= now && (selected < 0 || timer->due < events->timers[selected].due)) selected = i;
            if (timer->due < wake) wake = timer->due;
        }
        if (selected >= 0) {
            ons_timer *timer = &events->timers[selected];
            memset(event, 0, sizeof(*event)); event->type = ONS_EVENT_TIMER;
            event->code = timer->code; event->timestamp = now;
            if (timer->period) timer->due += ((now - timer->due) / timer->period + 1) * timer->period;
            else timer->active = 0;
            pthread_mutex_unlock(&events->lock); return 1;
        }
        if (now >= deadline) { pthread_mutex_unlock(&events->lock); return 0; }
        struct timespec when = {(time_t)(wake / 1000), (long)(wake % 1000) * 1000000};
        int rc = pthread_cond_timedwait(&events->changed, &events->lock, &when);
        if (rc && rc != ETIMEDOUT) { pthread_mutex_unlock(&events->lock); return -1; }
    }
}
