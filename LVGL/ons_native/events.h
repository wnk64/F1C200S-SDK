#ifndef ONS_NATIVE_EVENTS_H
#define ONS_NATIVE_EVENTS_H
#include <stdint.h>
#include <pthread.h>

enum ons_event_type { ONS_EVENT_TOUCH, ONS_EVENT_TIMER, ONS_EVENT_AUDIO_END, ONS_EVENT_QUIT };
typedef struct { enum ons_event_type type; int x, y, pressed, code; uint64_t timestamp; } ons_event;
typedef struct { int active, code; uint64_t due, period; } ons_timer;
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    ons_event pending[64];
    unsigned head, count;
    ons_timer timers[16];
    int closed;
} ons_events;
uint64_t ons_monotonic_ms(void);
int ons_events_open(ons_events *events);
void ons_events_close(ons_events *events);
void ons_events_destroy(ons_events *events);
int ons_events_push(ons_events *events, ons_event event);
/* Returns 1 for an event, 0 on timeout/closed queue, -1 on an OS failure. */
int ons_events_wait(ons_events *events, ons_event *event, unsigned timeout_ms);
int ons_timer_start(ons_events *events, int code, unsigned delay_ms, unsigned period_ms);
void ons_timer_cancel(ons_events *events, int code);
#endif
