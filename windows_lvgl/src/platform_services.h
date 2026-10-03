#pragma once

#include "h264_output.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

// Platform-only services used by the LVGL application. Page layout, routing,
// and response parsing must not depend on a desktop windowing or HTTP API.
enum class PlatformEventType { Quit, TextInput, KeyDown, PointerMove, PointerButton, PointerWheel };
enum class PlatformKey { Unknown, Backspace, Enter, Escape, Refresh, Popular, Home,
                         Left, Right, Up, Down, Fullscreen };

struct PlatformEvent {
    PlatformEventType type{};
    PlatformKey key{PlatformKey::Unknown};
    int x{};
    int y{};
    int wheel_y{};
    bool pressed{};
    std::string text;
};

enum class PlatformVideoInput { H264AnnexB, ContainerStream };

struct PlatformPlayerSpec {
    std::string video_id;
    std::string stream_uri;
    std::string audio_stream_uri;
    PlatformVideoInput input{PlatformVideoInput::H264AnnexB};
    uint16_t decode_width{};
    uint16_t decode_height{};
    uint16_t rotation_degrees{};
    uint16_t output_width{};
    uint16_t output_height{};
    int video_quality{};
    int video_format{4048};
    int video_codec{7};
    int audio_quality{};
    int memory_cache_mib{};
    int playback_speed_percent{100};
    int volume_percent{70};
    std::string aspect{"default"};
    bool low_quality{};
    bool tv_osd{};
    bool separate_video_plane{true};
    bool native_size_only{};
};

struct _lv_font_t;

bool platform_display_init(int width, int height, const char* title);
void platform_display_set_fullscreen(bool enabled);
void platform_display_shutdown();
bool platform_display_flush_pixels(const void* pixels, int x, int y, int width, int height, bool present);
bool platform_poll_event(PlatformEvent& event);
void platform_start_text_input();
void platform_stop_text_input();
uint32_t platform_ticks();
void platform_delay(uint32_t milliseconds);
bool platform_decode_thumbnail_lvgl(const std::vector<uint8_t>& encoded, int width, int height,
                                    std::vector<uint8_t>& pixels, bool* has_alpha = nullptr);
void platform_font_init();
void platform_font_shutdown();
const struct _lv_font_t* platform_ui_font(const struct _lv_font_t* fallback);

std::string platform_config_path();
void platform_set_http_proxy(const std::string& address, bool enabled);
void platform_set_http_timeouts(uint32_t connect_ms, uint32_t receive_ms);
void platform_set_tls_verify(bool enabled);
void platform_set_gamepad_ab_swap(bool enabled);
void platform_open_external_url(const std::string& url);
bool platform_https_get(const std::string& url, const std::string& headers,
                        std::vector<uint8_t>& body, std::string& error,
                        std::string* response_headers = nullptr,
                        size_t max_body_bytes = 2U * 1024U * 1024U);
bool platform_https_post(const std::string& url, const std::string& headers,
                         const std::string& form_body, std::vector<uint8_t>& body,
                         std::string& error);
bool platform_md5_hex(const std::string& source, std::string& digest, std::string& error);

bool platform_player_prepare(const PlatformPlayerSpec& spec, std::string& error);
bool platform_player_start(std::string& error);
bool platform_player_pause(std::string& error);
bool platform_player_seek(uint32_t position_ms, std::string& error);
bool platform_player_set_volume(uint8_t percent, std::string& error);
bool platform_player_set_speed(uint16_t percent, std::string& error);
bool platform_player_finished();
void platform_player_stop();
#ifdef WILIWILI_BOARD
void platform_player_controls_visible(bool visible);
#endif
