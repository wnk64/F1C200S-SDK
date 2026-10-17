#include <assert.h>
#include <stdio.h>
#include <string.h>

enum transport_state {
    TRANSPORT_STOPPED, TRANSPORT_PLAYING, TRANSPORT_PAUSED_PLAYBACK,
    TRANSPORT_NO_MEDIA_PRESENT, TRANSPORT_TRANSITIONING,
    TRANSPORT_PAUSED_RECORDING, TRANSPORT_RECORDING
};
enum {
    TRANSPORT_VAR_REL_TIME_POS, TRANSPORT_VAR_AV_URI,
    TRANSPORT_VAR_AV_URI_META, TRANSPORT_VAR_CUR_TRANSPORT_ACTIONS,
    UPNP_TRANSPORT_E_TRANSITION_NA
};
struct action_event { const char *uri; };
static enum transport_state transport_state_;
static int child_running, starts;
static const char *kZeroTime = "0:00:00";
static int has_instance_id(struct action_event *event) { return event != NULL; }
static const char *upnp_get_string(struct action_event *event, const char *name) {
    return strcmp(name, "CurrentURI") == 0 ? event->uri : "";
}
static void service_lock(void) {}
static void service_unlock(void) {}
static int replace_transport_uri_and_meta(const char *uri, const char *meta) { (void)uri; (void)meta; return 0; }
static void replace_current_uri_and_meta(const char *uri, const char *meta) { (void)uri; (void)meta; }
static void update_meta_from_stream(void) {}
static void output_set_uri(const char *uri, void (*cb)(void)) { (void)uri; (void)cb; child_running = 0; }
static void change_transport_state(enum transport_state state) { transport_state_ = state; }
static void replace_var(int var, const char *value) { (void)var; (void)value; }
static const char *get_var(int var) { (void)var; return ""; }
static void inform_play_transition_from_output(void) {}
static int output_play(void (*cb)(void)) { (void)cb; ++starts; child_running = 1; return 0; }
static void upnp_set_error(struct action_event *event, int code, const char *format, ...) { (void)event; (void)code; (void)format; }

#include "transport_actions.inc"

int main(void) {
    struct action_event event = {"http://example.invalid/new-video.mp4"};
    const enum transport_state states[] = {TRANSPORT_PLAYING, TRANSPORT_PAUSED_PLAYBACK, TRANSPORT_STOPPED};
    for (unsigned state = 0; state < sizeof(states) / sizeof(states[0]); ++state) {
        transport_state_ = states[state]; child_running = 1; starts = 0;
        for (int change = 0; change < 5; ++change) {
            assert(set_avtransport_uri(&event) == 0);
            assert(transport_state_ == TRANSPORT_STOPPED && !child_running);
            assert(play(&event) == 0);
            assert(transport_state_ == TRANSPORT_PLAYING && child_running);
            assert(starts == change + 1);
            assert(play(&event) == 0 && starts == change + 1);
        }
    }
    puts("PASS playing/paused/stopped URI replacement and repeated Play");
    return 0;
}
