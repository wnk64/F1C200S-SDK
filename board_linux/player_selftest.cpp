#include "platform_services.h"
#include <iostream>
#include <cstdlib>

static void require(bool passed, const std::string& message) {
    if (!passed) {
        std::cerr << "SELFTEST FAIL: " << message << std::endl;
        platform_player_stop();
        platform_display_shutdown();
        std::exit(1);
    }
}
static uint32_t wait_frame(uint32_t minimum, uint32_t maximum) {
    uint32_t started = platform_ticks(), position = 0;
    while (platform_ticks() - started < 45000) {
        require(!platform_player_finished(), "player unexpectedly exited");
        if (platform_player_ready() && platform_player_video_position(position) && position >= minimum && position <= maximum) {
            std::cout << "SELFTEST frame position_ms=" << position << std::endl;
            return position;
        }
        platform_delay(50);
    }
    require(false, "timed out waiting for committed video position");
    return 0;
}
int main() {
    std::string uri, error;
    std::getline(std::cin, uri);
    if (!uri.empty() && uri.back() == '\r') uri.pop_back();
    require(std::getenv("F1_PLAYER_DEBUG") && std::string(std::getenv("F1_PLAYER_DEBUG")) == "1", "explicit F1_PLAYER_DEBUG=1 required");
    require(platform_display_init(800, 480, "selftest"), "display init failed");
    PlatformPlayerSpec spec;
    spec.stream_uri = uri;
    spec.input = PlatformVideoInput::ContainerStream;
    spec.rotation_degrees = 90;
    spec.volume_percent = 40;
    require(platform_player_prepare(spec, error), error);
    require(platform_player_start(error), error);
    wait_frame(0, 20000);
    require(platform_player_set_volume(0, error), error);
    platform_delay(250);
    require(platform_player_set_volume(40, error), error);
    const bool audio_test = std::getenv("F1_AUDIO_RECOVERY_TEST") != nullptr;
    for (int cycle = 0; !audio_test && cycle < 2; ++cycle) {
        require(platform_player_pause(error), error);
        platform_delay(250);
        require(platform_player_seek(195000, error), error);
        wait_frame(194900, 201000);
        uint32_t started = platform_ticks();
        bool loop = false;
        while (platform_ticks() - started < 45000) {
            require(!platform_player_finished(), "player exited before looping");
            if (platform_player_rewound()) { loop = true; break; }
            platform_delay(50);
        }
        require(loop, "loop event timeout");
        wait_frame(0, 5000);
        wait_frame(5500, 13000);
        std::cout << "SELFTEST cycle=" << cycle + 1 << " loop-and-postloop-progress=PASS" << std::endl;
    }
    uint32_t before = 0, after = 0;
    require(platform_player_pause(error), error);
    platform_player_controls_visible(true);
    platform_delay(300);
    require(platform_player_video_position(before), "missing paused position");
    platform_delay(1200);
    require(platform_player_video_position(after) && after == before, "paused progress changed");
    require(platform_player_start(error), error);
    wait_frame(after + 500, after + 5000);
    if (audio_test) {
        std::cout << "SELFTEST audio-route observation window=20s" << std::endl;
        uint32_t position = 0;
        require(platform_player_video_position(position), "missing audio test position");
        wait_frame(position + 18000, position + 24000);
    }
    platform_player_stop();
    platform_display_shutdown();
    std::cout << "SELFTEST PASS: two seek-loop cycles, pause, resume, frozen progress" << std::endl;
}
