#include "platform_services.h"
extern "C" {
#include "lvgl.h"
extern const lv_font_t menu_font_stream;
void menu_font_stream_close(void);
int board_jpeg_thumbnail(const uint8_t *, size_t, int, int, uint16_t *);
}
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "vendor/httplib.h"
#include "geometry.h"
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/input.h>
#include <mutex>
#include <openssl/md5.h>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace {
int fb_fd = -1, input_fd = -1, i2c_fd = -1;
uint8_t *framebuffer = nullptr;
fb_var_screeninfo screen{};
fb_fix_screeninfo fixed{};
volatile sig_atomic_t stopped = 0;
int touch_x = 0, touch_y = 0, slot = 0;
bool pressed = false, last_pressed = false;
int last_x = -1, last_y = -1;
uint32_t last_irq = 0, last_poll = 0, start_tick = 0, test_duration = 0;
struct NetworkConfig { bool verify = true, proxy_enabled = false; std::string proxy; uint32_t connect_ms = 10000, receive_ms = 10000; };
NetworkConfig network;
std::mutex network_mutex;
void signal_stop(int) { stopped = 1; }

bool request(const std::string& url, const std::string& raw_headers,
             std::vector<uint8_t>& body, std::string& error, std::string *out_headers, size_t cap) {
    body.clear(); error.clear(); if (out_headers) out_headers->clear();
    if (url.compare(0, 8, "https://") != 0 || url.find_first_of("\r\n") != std::string::npos ||
        cap == 0 || cap > 2U * 1024U * 1024U) { error = "Invalid HTTPS request"; return false; }
    const size_t slash = url.find('/', 8);
    const std::string host = url.substr(8, slash == std::string::npos ? slash : slash - 8);
    if (host.empty() || host.find_first_of("@:?# ") != std::string::npos) { error = "Invalid HTTPS host"; return false; }
    const std::string path = slash == std::string::npos ? "/" : url.substr(slash);
    NetworkConfig config;
    { std::lock_guard<std::mutex> lock(network_mutex); config = network; }
    httplib::SSLClient client(host, 443);
    client.enable_server_certificate_verification(config.verify);
    const char *ca = std::getenv("F1_CA_FILE");
    client.set_ca_cert_path(ca ? ca : "/etc/ssl/certs/ca-certificates.crt");
    client.set_connection_timeout(config.connect_ms / 1000, (config.connect_ms % 1000) * 1000);
    client.set_read_timeout(config.receive_ms / 1000, (config.receive_ms % 1000) * 1000);
    client.set_write_timeout(config.receive_ms / 1000, (config.receive_ms % 1000) * 1000);
    client.set_follow_location(false);
    if (config.proxy_enabled) {
        const size_t colon = config.proxy.rfind(':');
        if (colon == std::string::npos) { error = "Proxy must be host:port"; return false; }
        const std::string port_text = config.proxy.substr(colon + 1);
        if (port_text.empty() || port_text.find_first_not_of("0123456789") != std::string::npos || port_text.size() > 5) {
            error = "Invalid proxy port"; return false;
        }
        const int port = std::atoi(port_text.c_str());
        if (port < 1 || port > 65535) { error = "Invalid proxy port"; return false; }
        client.set_proxy(config.proxy.substr(0, colon), port);
    }
    httplib::Headers headers;
    std::istringstream lines(raw_headers);
    for (std::string line; std::getline(lines, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        const size_t value = line.find_first_not_of(" \t", colon + 1);
        headers.emplace(line.substr(0, colon), value == std::string::npos ? "" : line.substr(value));
    }
    bool overflow = false;
    auto receiver = [&](const char *data, size_t size) {
        if (size > cap - body.size()) { overflow = true; return false; }
        body.insert(body.end(), data, data + size); return true;
    };
    auto handler = [&](const httplib::Response& response) {
        if (out_headers) {
            for (const auto& item : response.headers) {
                if (out_headers->size() + item.first.size() + item.second.size() + 4 > 32768) { overflow = true; return false; }
                *out_headers += item.first + ": " + item.second + "\r\n";
            }
        }
        if (response.has_header("Content-Length")) {
            const auto text = response.get_header_value("Content-Length");
            if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos ||
                text.size() > 10 || std::strtoull(text.c_str(), nullptr, 10) > cap) { overflow = true; return false; }
        }
        return true;
    };
    httplib::Result result = client.Get(path.c_str(), headers, handler, receiver);
    if (!result || overflow) { body.clear(); error = overflow ? "HTTPS response exceeds limit" : "HTTPS transport failed"; return false; }
    if (result->status < 200 || result->status >= 300) {
        body.clear(); error = "HTTP status " + std::to_string(result->status); return false;
    }
    return true;
}
}

uint32_t platform_ticks() {
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
void platform_delay(uint32_t milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }
bool platform_display_init(int width, int height, const char *) {
    if (width != board::width || height != board::height) return false;
    stopped = 0; std::signal(SIGTERM, signal_stop); std::signal(SIGINT, signal_stop);
    fb_fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
    if (fb_fd < 0 || ioctl(fb_fd, FBIOGET_VSCREENINFO, &screen) || ioctl(fb_fd, FBIOGET_FSCREENINFO, &fixed)) {
        platform_display_shutdown(); return false;
    }
    const uint64_t needed = static_cast<uint64_t>(screen.yoffset + board::panel_height) * fixed.line_length;
    if (screen.xres != board::panel_width || screen.yres != board::panel_height || screen.bits_per_pixel != 32 ||
        screen.red.length != 8 || screen.green.length != 8 || screen.blue.length != 8 ||
        screen.red.offset != 16 || screen.green.offset != 8 || screen.blue.offset != 0 ||
        fixed.line_length < (screen.xoffset + board::panel_width) * 4U || needed > fixed.smem_len) {
        platform_display_shutdown(); return false;
    }
    void *mapped = mmap(nullptr, fixed.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (mapped == MAP_FAILED) { platform_display_shutdown(); return false; }
    framebuffer = static_cast<uint8_t *>(mapped);
    for (int n = 0; n < 8; ++n) {
        const std::string path = "/dev/input/event" + std::to_string(n);
        const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        char name[128]{};
        if (fd >= 0 && ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0 &&
            (std::strstr(name, "ft6") || std::strstr(name, "FT6") || std::strstr(name, "Focal"))) { input_fd = fd; break; }
        if (fd >= 0) close(fd);
    }
    i2c_fd = open("/dev/i2c-0", O_RDWR | O_CLOEXEC);
    start_tick = platform_ticks();
    const char *duration = std::getenv("F1_UI_TEST_MS");
    if (duration) test_duration = std::min(60000U, static_cast<unsigned>(std::strtoul(duration, nullptr, 10)));
    return true;
}
void platform_display_set_fullscreen(bool) {}
void platform_display_shutdown() {
    if (framebuffer) munmap(framebuffer, fixed.smem_len);
    framebuffer = nullptr;
    for (int *fd : {&input_fd, &i2c_fd, &fb_fd}) { if (*fd >= 0) close(*fd); *fd = -1; }
}
bool platform_display_flush_pixels(const void *data, int x, int y, int width, int height, bool) {
    if (!framebuffer || !data || !board::rectangle(x, y, width, height)) return false;
    static_assert(sizeof(lv_color_t) == 2, "board requires RGB565 strips");
    const auto *pixels = static_cast<const lv_color_t *>(data);
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            int px, py; board::to_panel(x + col, y + row, px, py);
            auto *out = reinterpret_cast<uint32_t *>(framebuffer + (py + screen.yoffset) * fixed.line_length);
            out[px + screen.xoffset] = board::rgb565(pixels[row * width + col].full);
        }
    }
    return true;
}
bool platform_poll_event(PlatformEvent& event) {
    const uint32_t now = platform_ticks();
    if (stopped || (test_duration && now - start_tick >= test_duration)) { event.type = PlatformEventType::Quit; stopped = 0; test_duration = 0; return true; }
    input_event e{};
    while (input_fd >= 0 && read(input_fd, &e, sizeof(e)) == sizeof(e)) {
        if (e.type == EV_ABS && e.code == ABS_MT_SLOT) slot = e.value;
        if (e.type == EV_ABS && slot == 0) {
            if (e.code == ABS_MT_POSITION_X || e.code == ABS_X) touch_y = 479 - e.value;
            if (e.code == ABS_MT_POSITION_Y || e.code == ABS_Y) touch_x = e.value;
            if (e.code == ABS_MT_TRACKING_ID) pressed = e.value >= 0;
        }
        if (e.type == EV_KEY && e.code == BTN_TOUCH) pressed = e.value != 0;
        if (e.type == EV_SYN && e.code == SYN_REPORT) last_irq = now;
    }
    if (i2c_fd >= 0 && now - last_irq > 500 && now - last_poll >= 20) {
        last_poll = now;
        uint8_t reg = 2, report[13]{};
        i2c_msg messages[2]{};
        messages[0].addr = 0x38; messages[0].len = 1; messages[0].buf = &reg;
        messages[1].addr = 0x38; messages[1].flags = I2C_M_RD; messages[1].len = sizeof(report); messages[1].buf = report;
        i2c_rdwr_ioctl_data transfer{messages, 2};
        if (ioctl(i2c_fd, I2C_RDWR, &transfer) == 2) {
            pressed = (report[0] & 15) != 0;
            if (pressed) { touch_x = ((report[1] & 15) << 8) | report[2]; touch_y = ((report[3] & 15) << 8) | report[4]; }
        } else pressed = false;
    }
    touch_x = std::max(0, std::min(799, touch_x)); touch_y = std::max(0, std::min(479, touch_y));
    if (touch_x == last_x && touch_y == last_y && pressed == last_pressed) return false;
    event.type = PlatformEventType::PointerButton; event.x = touch_x; event.y = touch_y; event.pressed = pressed;
    last_x = touch_x; last_y = touch_y; last_pressed = pressed; return true;
}
void platform_start_text_input() {}
void platform_stop_text_input() {}
bool platform_decode_thumbnail_lvgl(const std::vector<uint8_t>& encoded, int width, int height,
                                    std::vector<uint8_t>& pixels, bool *alpha) {
    if (alpha) *alpha = false;
    if (width < 1 || height < 1 || width > 256 || height > 256) return false;
    pixels.resize(static_cast<size_t>(width) * height * 2);
    if (!board_jpeg_thumbnail(encoded.data(), encoded.size(), width, height, reinterpret_cast<uint16_t *>(pixels.data()))) { pixels.clear(); return false; }
    return true;
}
void platform_font_init() {}
void platform_font_shutdown() { menu_font_stream_close(); }
const _lv_font_t *platform_ui_font(const _lv_font_t *) { return &menu_font_stream; }
std::string platform_config_path() {
    const char *directory = std::getenv("F1_CONFIG_DIR");
    const std::string path = directory ? directory : "/root/.config/wiliwili-lvgl";
    (void)mkdir("/root/.config", 0700); (void)mkdir(path.c_str(), 0700); return path;
}
void platform_set_http_proxy(const std::string& proxy, bool enabled) { std::lock_guard<std::mutex> lock(network_mutex); network.proxy = proxy; network.proxy_enabled = enabled; }
void platform_set_http_timeouts(uint32_t connect_ms, uint32_t receive_ms) { std::lock_guard<std::mutex> lock(network_mutex); network.connect_ms = std::max(1000U, std::min(30000U, connect_ms)); network.receive_ms = std::max(1000U, std::min(30000U, receive_ms)); }
void platform_set_tls_verify(bool verify) { std::lock_guard<std::mutex> lock(network_mutex); network.verify = verify; }
void platform_set_gamepad_ab_swap(bool) {}
void platform_open_external_url(const std::string&) {}
bool platform_https_get(const std::string& url, const std::string& headers, std::vector<uint8_t>& body,
                        std::string& error, std::string *response_headers, size_t cap) {
    return request(url, headers, body, error, response_headers, cap);
}
bool platform_https_post(const std::string&, const std::string&, const std::string&, std::vector<uint8_t>& body, std::string& error) {
    body.clear(); error = "POST adapter not enabled in this candidate"; return false;
}
bool platform_md5_hex(const std::string& input, std::string& digest, std::string&) {
    unsigned char bytes[MD5_DIGEST_LENGTH]; MD5(reinterpret_cast<const unsigned char *>(input.data()), input.size(), bytes);
    static const char hex[] = "0123456789abcdef"; digest.resize(32);
    for (size_t i = 0; i < 16; ++i) { digest[i * 2] = hex[bytes[i] >> 4]; digest[i * 2 + 1] = hex[bytes[i] & 15]; }
    return true;
}
bool platform_player_prepare(const PlatformPlayerSpec&, std::string& error) { error = "Cedar container-stream adapter is not ported yet"; return false; }
bool platform_player_start(std::string& error) { error = "Cedar container-stream adapter is not ported yet"; return false; }
bool platform_player_pause(std::string& error) { error = "Player is not active"; return false; }
bool platform_player_seek(uint32_t, std::string& error) { error = "Player is not active"; return false; }
bool platform_player_set_volume(uint8_t, std::string& error) { error = "Player is not active"; return false; }
bool platform_player_set_speed(uint16_t, std::string& error) { error = "Player is not active"; return false; }
bool platform_player_finished() { return true; }
void platform_player_stop() {}
