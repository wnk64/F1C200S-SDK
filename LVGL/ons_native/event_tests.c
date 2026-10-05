#define _POSIX_C_SOURCE 200809L
#include "events.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>

static void *producer(void *argument)
{
    ons_events *events = argument;
    struct timespec pause = {0, 20000000}; nanosleep(&pause, NULL);
    ons_event event = {.type = ONS_EVENT_AUDIO_END, .code = 9};
    assert(!ons_events_push(events, event)); return NULL;
}
int main(void)
{
    ons_events events; ons_event received;
    assert(!ons_events_open(&events));
    for (int i = 0; i < 64; ++i) {
        ons_event event = {.type = ONS_EVENT_TOUCH, .code = i, .pressed = i % 2};
        assert(!ons_events_push(&events, event));
    }
    ons_event full = {.type = ONS_EVENT_QUIT}; assert(ons_events_push(&events, full) == -1);
    for (int i = 0; i < 64; ++i) assert(ons_events_wait(&events, &received, 0) == 1 && received.code == i);
    assert(ons_events_wait(&events, &received, 0) == 0);
    assert(!ons_timer_start(&events, 7, 20, 0));
    uint64_t start = ons_monotonic_ms();
    assert(ons_events_wait(&events, &received, 1000) == 1 && received.type == ONS_EVENT_TIMER && received.code == 7);
    assert(ons_monotonic_ms() - start >= 19);
    assert(ons_events_wait(&events, &received, 0) == 0);
    assert(!ons_timer_start(&events, 8, 0, 10));
    assert(ons_events_wait(&events, &received, 0) == 1 && received.code == 8);
    assert(ons_events_wait(&events, &received, 1000) == 1 && received.code == 8);
    ons_timer_cancel(&events, 8); assert(ons_events_wait(&events, &received, 20) == 0);
    for (int i = 0; i < 16; ++i) assert(!ons_timer_start(&events, 100 + i, 60000, 0));
    assert(ons_timer_start(&events, 200, 60000, 0) == -1);
    assert(!ons_timer_start(&events, 100, 60000, 0));
    for (int i = 0; i < 16; ++i) ons_timer_cancel(&events, 100 + i);
    pthread_t thread; assert(!pthread_create(&thread, NULL, producer, &events));
    assert(ons_events_wait(&events, &received, 1000) == 1 && received.type == ONS_EVENT_AUDIO_END && received.code == 9);
    assert(!pthread_join(thread, NULL));
    ons_events_close(&events);
    assert(ons_events_wait(&events, &received, 1000) == 0);
    assert(ons_events_push(&events, full) == -1);
    assert(ons_timer_start(&events, 1, 0, 0) == -1);
    ons_events_destroy(&events);
    puts("Native FIFO/bounds/timer/cancel/thread/close tests PASS (no SDL)"); return 0;
}
