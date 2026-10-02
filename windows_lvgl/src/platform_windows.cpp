#include "platform_services.h"
#include "bounded_http_body.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_syswm.h>
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <winhttp.h>

#include "lvgl.h"
#include "extra/libs/freetype/lv_freetype.h"

#include <cstring>
#include <algorithm>
#include <fstream>

namespace {

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_Texture* texture = nullptr;
lv_ft_info_t chinese_font{};
PROCESS_INFORMATION h264_test_player{};
std::wstring h264_test_source;
std::wstring h264_test_audio_source;
int h264_test_volume = 70;
int h264_test_speed = 100;
std::wstring h264_test_aspect;
uint32_t h264_test_seek_ms = 0;
bool h264_test_paused = false;
bool h264_test_raw_annexb = true;
std::wstring http_proxy_address;
bool http_proxy_enabled = false;
bool tls_verify_enabled = true;
uint32_t http_connect_timeout_ms = 10000;
uint32_t http_receive_timeout_ms = 15000;
bool gamepad_ab_swapped = false;
UINT ui_audit_message = 0;
WNDPROC ui_audit_original_proc = nullptr;
HWND ui_audit_window = nullptr;
const uint32_t* ui_audit_pixels = nullptr;
std::vector<uint32_t> ui_audit_frame;
uint32_t ui_audit_present_count = 0;
int display_width = 0;
int display_height = 0;
int ui_audit_width = 0;
int ui_audit_height = 0;
int ui_audit_pointer_x = 0;
int ui_audit_pointer_y = 0;
uint32_t ui_audit_lvgl_sampled_peak = 0;

uint32_t image_integer(const std::vector<uint8_t>& data, size_t offset, size_t count, bool big_endian = false) {
    uint32_t result = 0;
    for (size_t byte_index = 0; byte_index < count; ++byte_index) {
        const size_t shift = (big_endian ? count - byte_index - 1 : byte_index) * 8;
        result |= static_cast<uint32_t>(data[offset + byte_index]) << shift;
    }
    return result;
}

bool bounded_thumbnail_dimensions(const std::vector<uint8_t>& data) {
    uint32_t width = 0;
    uint32_t height = 0;
    if (data.size() >= 24 && std::memcmp(data.data(), "\x89PNG\r\n\x1a\n", 8) == 0 &&
        std::memcmp(data.data() + 12, "IHDR", 4) == 0) {
        width = image_integer(data, 16, 4, true);
        height = image_integer(data, 20, 4, true);
    } else if (data.size() >= 26 && data[0] == 'B' && data[1] == 'M') {
        const uint32_t header_size = image_integer(data, 14, 4);
        if (header_size == 12) {
            width = image_integer(data, 18, 2);
            height = image_integer(data, 20, 2);
        } else if (header_size >= 40) {
            width = image_integer(data, 18, 4);
            height = image_integer(data, 22, 4);
            if (height & 0x80000000U) height = ~height + 1U;
        }
    } else if (data.size() >= 4 && data[0] == 0xff && data[1] == 0xd8) {
        size_t cursor = 2;
        while (cursor < data.size()) {
            if (data[cursor++] != 0xff) return false;
            while (cursor < data.size() && data[cursor] == 0xff) ++cursor;
            if (cursor >= data.size()) return false;
            const uint8_t marker = data[cursor++];
            if (marker == 0xda || marker == 0xd9) break;
            if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd8)) continue;
            if (data.size() - cursor < 2) return false;
            const uint32_t length = image_integer(data, cursor, 2, true);
            if (length < 2 || length > data.size() - cursor) return false;
            if (marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 && marker != 0xc8 && marker != 0xcc) {
                if (length < 8) return false;
                height = image_integer(data, cursor + 3, 2, true);
                width = image_integer(data, cursor + 5, 2, true);
                break;
            }
            cursor += length;
        }
    } else if (data.size() >= 30 && std::memcmp(data.data(), "RIFF", 4) == 0 &&
               std::memcmp(data.data() + 8, "WEBP", 4) == 0) {
        if (std::memcmp(data.data() + 12, "VP8X", 4) == 0) {
            width = image_integer(data, 24, 3) + 1U;
            height = image_integer(data, 27, 3) + 1U;
        } else if (std::memcmp(data.data() + 12, "VP8 ", 4) == 0 &&
                   data[23] == 0x9d && data[24] == 0x01 && data[25] == 0x2a) {
            width = image_integer(data, 26, 2) & 0x3fffU;
            height = image_integer(data, 28, 2) & 0x3fffU;
        } else if (std::memcmp(data.data() + 12, "VP8L", 4) == 0 && data[20] == 0x2f) {
            const uint32_t packed = image_integer(data, 21, 4);
            width = (packed & 0x3fffU) + 1U;
            height = ((packed >> 14) & 0x3fffU) + 1U;
        }
    }
    return width > 0 && height > 0 && width <= 2048 && height <= 2048 &&
           static_cast<uint64_t>(width) * height <= 512U * 1024U;
}

bool ui_audit_enabled() {
    char enabled[2]{};
    return GetEnvironmentVariableA("WILIWILI_UI_AUDIT", enabled, sizeof(enabled)) == 1 && enabled[0] == '1';
}

std::string ui_audit_directory() {
    char directory[32768]{};
    const DWORD length = GetEnvironmentVariableA("WILIWILI_UI_AUDIT_DIR", directory, sizeof(directory));
    return length && length < sizeof(directory) ? directory : "";
}

std::string ui_audit_fixture_directory() {
    if (!ui_audit_enabled()) return {};
    char directory[32768]{};
    const DWORD length = GetEnvironmentVariableA("WILIWILI_UI_AUDIT_FIXTURES", directory, sizeof(directory));
    return length && length < sizeof(directory) ? directory : "";
}

bool ui_audit_fixture_get(const std::string& url, std::vector<uint8_t>& body, std::string& error,
                          size_t max_body_bytes) {
    std::string filename = "empty.json";
    if (url.find("/x/web-interface/search/type") != std::string::npos) {
        if (url.find("search_type=video") != std::string::npos) {
            filename = url.find("page=1&") != std::string::npos ? "search_1.json" : "search_2.json";
        }
    } else if (url.find("/x/web-interface/wbi/index/top/feed/rcmd") != std::string::npos ||
               url.find("/x/web-interface/popular") != std::string::npos) {
        filename = "feed.json";
        Sleep(800);
    } else if (url.find("/x/web-interface/view?bvid=") != std::string::npos) {
        const size_t start = url.find("bvid=") + 5;
        const size_t end = url.find('&', start);
        const std::string video_id = url.substr(start, end == std::string::npos ? end : end - start);
        if (video_id.empty() || !std::all_of(video_id.begin(), video_id.end(), [](unsigned char character) {
                return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
                       (character >= '0' && character <= '9'); })) {
            error = "Invalid fixture video ID";
            return false;
        }
        filename = "view_" + video_id + ".json";
    } else if (url.find("/x/web-interface/search/square") != std::string::npos) {
        filename = "hots.json";
        Sleep(800);
    } else if (url == "https://api.bilibili.com/x/web-interface/nav") {
        filename = "nav.json";
    } else if (url.find("/x/polymer/web-dynamic/v1/uplist") != std::string::npos) {
        filename = "dynamic_ups.json";
    } else if (url.find("/x/polymer/web-dynamic/desktop/v1/feed/video") != std::string::npos) {
        filename = "dynamic_videos.json";
        Sleep(350);
    } else if (url.find("/x/polymer/web-dynamic/desktop/v1/feed/all") != std::string::npos) {
        filename = url.find("host_mid=42") == std::string::npos ? "dynamic_articles.json" : "dynamic_creator.json";
    } else if (url == "https://passport.bilibili.com/x/passport-login/web/qrcode/generate?source=main_electron_pc") {
        filename = "qr_generate.json";
    } else if (url == "https://passport.bilibili.com/x/passport-login/web/qrcode/poll?qrcode_key=offline-test-key&source=main_electron_pc") {
        filename = "qr_poll.json";
    } else if (url.find("https://i0.hdslb.com/bfs/archive/cover_") == 0) {
        const std::string suffix = "@256w_144h_1c.jpg";
        const size_t suffix_start = url.find(suffix);
        const size_t number_start = std::strlen("https://i0.hdslb.com/bfs/archive/cover_");
        if (suffix_start == std::string::npos || suffix_start + suffix.size() != url.size() ||
            suffix_start <= number_start || suffix_start - number_start > 2 ||
            url.substr(number_start, suffix_start - number_start).find_first_not_of("0123456789") != std::string::npos) {
            error = "Invalid resized cover URL";
            return false;
        }
        filename = "cover_" + url.substr(number_start, suffix_start - number_start) + ".bmp";
    } else if (url.find("https://audit.invalid/cover_") == 0) {
        filename = url.substr(std::strlen("https://audit.invalid/"));
        if (filename.find_first_not_of("cover_0123456789.bmp") != std::string::npos ||
            filename.find("..") != std::string::npos) {
            error = "Invalid fixture cover path";
            return false;
        }
    }
    std::ifstream input(ui_audit_fixture_directory() + "/" + filename, std::ios::binary | std::ios::ate);
    if (!input) {
        error = "Missing background fixture: " + filename;
        return false;
    }
    const std::streampos size = input.tellg();
    if (size < 0 || static_cast<uint64_t>(size) > max_body_bytes) {
        error = "Background fixture exceeds response limit";
        return false;
    }
    body.resize(static_cast<size_t>(size));
    input.seekg(0);
    if (!body.empty() && !input.read(reinterpret_cast<char*>(body.data()), static_cast<std::streamsize>(body.size()))) {
        error = "Cannot read background fixture";
        body.clear();
        return false;
    }
    error.clear();
    return true;
}

HINTERNET open_https_session() {
    const DWORD access_type = http_proxy_enabled && !http_proxy_address.empty()
        ? WINHTTP_ACCESS_TYPE_NAMED_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
    const wchar_t* proxy = access_type == WINHTTP_ACCESS_TYPE_NAMED_PROXY ? http_proxy_address.c_str() : WINHTTP_NO_PROXY_NAME;
    HINTERNET session = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                       L"(KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36",
                       access_type, proxy, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session) WinHttpSetTimeouts(session, static_cast<int>(http_connect_timeout_ms),
                                    static_cast<int>(http_connect_timeout_ms),
                                    static_cast<int>(http_connect_timeout_ms),
                                    static_cast<int>(http_receive_timeout_ms));
    return session;
}

bool configure_tls(HINTERNET request) {
    if (tls_verify_enabled) return true;
    DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                  SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
    return WinHttpSetOption(request, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags)) != FALSE;
}

bool suspend_h264_process(bool suspend) {
    if (!h264_test_player.hProcess) return false;
    const DWORD pid = GetProcessId(h264_test_player.hProcess);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    bool changed = false;
    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID != pid) continue;
            HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, entry.th32ThreadID);
            if (!thread) continue;
            if (suspend) SuspendThread(thread);
            else ResumeThread(thread);
            CloseHandle(thread);
            changed = true;
        } while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return changed;
}

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (!length) return {};
    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wide.data(), length);
    wide.pop_back();
    return wide;
}

std::string wide_to_utf8(const wchar_t* value) {
    if (!value || !*value) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};
    std::string result(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), size - 1, nullptr, nullptr);
    return result;
}

LRESULT CALLBACK ui_audit_window_proc(HWND handle, UINT message, WPARAM operation, LPARAM value) {
    if (message != ui_audit_message) return CallWindowProc(ui_audit_original_proc, handle, message, operation, value);
    if (operation == 0) return 0x57494c49;
    if (operation == 6) {
        if (!ui_audit_pixels || value < 0) return 0;
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(const_cast<uint32_t*>(ui_audit_pixels),
            ui_audit_width, ui_audit_height, 32, ui_audit_width * sizeof(uint32_t), SDL_PIXELFORMAT_ARGB8888);
        if (!surface) return 0;
        const std::string path = ui_audit_directory() + "\\frame_" + std::to_string(value) + ".bmp";
        const bool saved = SDL_SaveBMP(surface, path.c_str()) == 0;
        SDL_FreeSurface(surface);
        return saved ? 1 : 0;
    }
    if (operation == 10) {
        if (!ui_audit_pixels || !texture || !renderer) return 0;
        int width = 0;
        int height = 0;
        if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0 ||
            width != ui_audit_width || height != ui_audit_height) return 0;
        std::vector<uint32_t> rendered(static_cast<size_t>(width) * height);
        if (SDL_RenderClear(renderer) != 0 || SDL_RenderCopy(renderer, texture, nullptr, nullptr) != 0 ||
            SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888,
                                 rendered.data(), width * sizeof(uint32_t)) != 0) return 0;
        for (size_t index = 0; index < rendered.size(); ++index) {
            for (int shift = 0; shift < 24; shift += 8) {
                const int difference = static_cast<int>((rendered[index] >> shift) & 255U) -
                                       static_cast<int>((ui_audit_pixels[index] >> shift) & 255U);
                if (difference < -7 || difference > 7) return 0;
            }
        }
        return 1;
    }
    if (operation == 9) {
        if (!ui_audit_pixels) return 0;
        lv_mem_monitor_t memory{};
        lv_mem_monitor(&memory);
        const uint32_t used = memory.total_size - memory.free_size;
        ui_audit_lvgl_sampled_peak = std::max(ui_audit_lvgl_sampled_peak, used);
        if (value == 0) return used;
        if (value == 1) return ui_audit_lvgl_sampled_peak;
        if (value == 2) return memory.frag_pct;
        if (value == 3) return memory.total_size;
        if (value == 4) return ui_audit_present_count;
        return 0;
    }
    SDL_Event event{};
    if (operation >= 1 && operation <= 3) {
        const int pointer_x = static_cast<uint16_t>(LOWORD(value));
        const int pointer_y = static_cast<uint16_t>(HIWORD(value));
        if (pointer_x >= ui_audit_width || pointer_y >= ui_audit_height) return 0;
        ui_audit_pointer_x = pointer_x;
        ui_audit_pointer_y = pointer_y;
        if (operation == 1) {
            event.type = SDL_MOUSEMOTION;
            event.motion.x = pointer_x;
            event.motion.y = pointer_y;
        } else {
            event.type = operation == 2 ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
            event.button.button = SDL_BUTTON_LEFT;
            event.button.state = operation == 2 ? SDL_PRESSED : SDL_RELEASED;
            event.button.x = pointer_x;
            event.button.y = pointer_y;
        }
    } else if (operation == 4) {
        event.type = SDL_KEYDOWN;
        switch (value) {
            case VK_ESCAPE: event.key.keysym.sym = SDLK_ESCAPE; break;
            case VK_RETURN: event.key.keysym.sym = SDLK_RETURN; break;
            case VK_BACK: event.key.keysym.sym = SDLK_BACKSPACE; break;
            case VK_LEFT: event.key.keysym.sym = SDLK_LEFT; break;
            case VK_RIGHT: event.key.keysym.sym = SDLK_RIGHT; break;
            case VK_UP: event.key.keysym.sym = SDLK_UP; break;
            case VK_DOWN: event.key.keysym.sym = SDLK_DOWN; break;
            case 'R': event.key.keysym.sym = SDLK_r; break;
            case 'P': event.key.keysym.sym = SDLK_p; break;
            case 'H': event.key.keysym.sym = SDLK_h; break;
            default: return 0;
        }
    } else if (operation == 5) {
        if (value <= 0 || value > 0xffff || (value >= 0xd800 && value <= 0xdfff)) return 0;
        const wchar_t character[] = {static_cast<wchar_t>(value), L'\0'};
        event.type = SDL_TEXTINPUT;
        const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, character, -1,
            event.text.text, sizeof(event.text.text), nullptr, nullptr);
        if (size <= 1) return 0;
    } else if (operation == 7) {
        event.type = SDL_QUIT;
    } else if (operation == 8) {
        if (value < -20 || value > 20) return 0;
        event.type = SDL_MOUSEWHEEL;
        event.wheel.y = static_cast<int>(value);
#if SDL_VERSION_ATLEAST(2, 26, 0)
        event.wheel.mouseX = ui_audit_pointer_x;
        event.wheel.mouseY = ui_audit_pointer_y;
#endif
    } else {
        return 0;
    }
    return SDL_PushEvent(&event) == 1 ? 1 : 0;
}

}  // namespace

bool platform_display_init(int width, int height, const char* title) {
    const bool audit = ui_audit_enabled();
    if (audit && ui_audit_directory().empty()) return false;
    // The board profile uses physical 800x480 pixels. Without DPI awareness,
    // Windows scales the SDL client area and clips the right-hand LVGL column.
    using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
    HMODULE user32 = GetModuleHandleA("user32.dll");
    auto set_dpi_context = user32 ? reinterpret_cast<SetProcessDpiAwarenessContextFn>(
        GetProcAddress(user32, "SetProcessDpiAwarenessContext")) : nullptr;
    if (set_dpi_context) set_dpi_context(reinterpret_cast<HANDLE>(-4));  // Per-monitor aware V2
    else SetProcessDPIAware();
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) return false;
    SDL_GameControllerEventState(SDL_ENABLE);
    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_WEBP);
    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              width, height,
                              SDL_WINDOW_ALLOW_HIGHDPI | (audit ? SDL_WINDOW_HIDDEN : 0));
    char software_mode[2]{};
    const bool software_audit = audit && GetEnvironmentVariableA("WILIWILI_UI_AUDIT_SOFTWARE", software_mode, sizeof(software_mode)) == 1 &&
                                software_mode[0] == '1';
    renderer = SDL_CreateRenderer(window, -1, software_audit ? SDL_RENDERER_SOFTWARE :
                                  SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer) SDL_RenderSetLogicalSize(renderer, width, height);
    texture = SDL_CreateTexture(renderer, LV_COLOR_DEPTH == 16 ? SDL_PIXELFORMAT_RGB565 : SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, width, height);
    if (window && renderer && texture) {
        display_width = width;
        display_height = height;
        if (audit) {
            SDL_SysWMinfo info{};
            SDL_VERSION(&info.version);
            if (!SDL_GetWindowWMInfo(window, &info)) {
                platform_display_shutdown();
                return false;
            }
            ui_audit_window = info.info.win.window;
            ui_audit_width = width;
            ui_audit_height = height;
            ui_audit_frame.assign(static_cast<size_t>(width) * height, 0);
            ui_audit_present_count = 0;
            ui_audit_message = RegisterWindowMessageA("Wiliwili.Lvgl.BackgroundAudit.v1");
            ui_audit_original_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(ui_audit_window, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(ui_audit_window_proc)));
            if (!ui_audit_message || !ui_audit_original_proc) {
                platform_display_shutdown();
                return false;
            }
            SetPropA(ui_audit_window, "WiliwiliLvglAudit", reinterpret_cast<HANDLE>(1));
        }
        return true;
    }
    platform_display_shutdown();
    return false;
}

void platform_display_shutdown() {
    if (ui_audit_window && ui_audit_original_proc) {
        SetWindowLongPtr(ui_audit_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ui_audit_original_proc));
        RemovePropA(ui_audit_window, "WiliwiliLvglAudit");
    }
    ui_audit_window = nullptr;
    ui_audit_original_proc = nullptr;
    ui_audit_pixels = nullptr;
    std::vector<uint32_t>().swap(ui_audit_frame);
    display_width = 0;
    display_height = 0;
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    texture = nullptr;
    renderer = nullptr;
    window = nullptr;
    IMG_Quit();
    SDL_Quit();
}

void platform_display_set_fullscreen(bool enabled) {
    if (window) SDL_SetWindowFullscreen(window, enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

bool platform_display_flush_pixels(const void* pixels, int x, int y, int width, int height, bool present) {
    if (!texture || !renderer || !pixels || x < 0 || y < 0 || width <= 0 || height <= 0 ||
        x > display_width || y > display_height || width > display_width - x || height > display_height - y) return false;
    const SDL_Rect rectangle{x, y, width, height};
    if (SDL_UpdateTexture(texture, &rectangle, pixels, width * static_cast<int>(sizeof(lv_color_t))) != 0) return false;
    if (ui_audit_window) {
        const auto* source = static_cast<const lv_color_t*>(pixels);
        for (int row = 0; row < height; ++row) {
            uint32_t* destination = ui_audit_frame.data() + static_cast<size_t>(y + row) * display_width + x;
            for (int column = 0; column < width; ++column)
                destination[column] = lv_color_to32(source[static_cast<size_t>(row) * width + column]);
        }
        ui_audit_pixels = ui_audit_frame.data();
    }
    if (present) {
        if (SDL_RenderClear(renderer) != 0 || SDL_RenderCopy(renderer, texture, nullptr, nullptr) != 0) return false;
        SDL_RenderPresent(renderer);
        if (ui_audit_window) ++ui_audit_present_count;
    }
    return true;
}

bool platform_poll_event(PlatformEvent& output) {
    SDL_Event event;
    if (!SDL_PollEvent(&event)) return false;
    if (event.type == SDL_QUIT) {
        output.type = PlatformEventType::Quit;
        return true;
    }
    if (event.type == SDL_TEXTINPUT) {
        output.type = PlatformEventType::TextInput;
        output.text = event.text.text;
        return true;
    }
    if (event.type == SDL_MOUSEMOTION) {
        output.type = PlatformEventType::PointerMove;
        output.x = event.motion.x;
        output.y = event.motion.y;
        return true;
    }
    if (event.type == SDL_MOUSEWHEEL) {
        output.type = PlatformEventType::PointerWheel;
        output.wheel_y = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
#if SDL_VERSION_ATLEAST(2, 26, 0)
        output.x = event.wheel.mouseX;
        output.y = event.wheel.mouseY;
#else
        SDL_GetMouseState(&output.x, &output.y);
#endif
        return true;
    }
    if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        output.type = PlatformEventType::PointerButton;
        output.x = event.button.x;
        output.y = event.button.y;
        output.pressed = event.type == SDL_MOUSEBUTTONDOWN;
        return true;
    }
    if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        output.type = PlatformEventType::KeyDown;
        switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: output.key = PlatformKey::Left; break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: output.key = PlatformKey::Right; break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP: output.key = PlatformKey::Up; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: output.key = PlatformKey::Down; break;
            case SDL_CONTROLLER_BUTTON_A: output.key = gamepad_ab_swapped ? PlatformKey::Escape : PlatformKey::Enter; break;
            case SDL_CONTROLLER_BUTTON_B: output.key = gamepad_ab_swapped ? PlatformKey::Enter : PlatformKey::Escape; break;
            default: output.key = PlatformKey::Unknown; break;
        }
        return output.key != PlatformKey::Unknown;
    }
    // Window/paint/device events are not application input.  Do not recurse
    // through the SDL queue here: a steady stream of those events can starve
    // the outer LVGL loop, leaving the window "Not responding" and preventing
    // completed feed requests from ever being rendered.
    if (event.type != SDL_KEYDOWN) return false;
    output.type = PlatformEventType::KeyDown;
    switch (event.key.keysym.sym) {
        case SDLK_BACKSPACE: output.key = PlatformKey::Backspace; break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: output.key = PlatformKey::Enter; break;
        case SDLK_ESCAPE: output.key = PlatformKey::Escape; break;
        case SDLK_r: output.key = PlatformKey::Refresh; break;
        case SDLK_p: output.key = PlatformKey::Popular; break;
        case SDLK_h: output.key = PlatformKey::Home; break;
        case SDLK_LEFT: output.key = PlatformKey::Left; break;
        case SDLK_RIGHT: output.key = PlatformKey::Right; break;
        case SDLK_UP: output.key = PlatformKey::Up; break;
        case SDLK_DOWN: output.key = PlatformKey::Down; break;
        case SDLK_F11: output.key = PlatformKey::Fullscreen; break;
        default: output.key = PlatformKey::Unknown; break;
    }
    return true;
}

void platform_start_text_input() { SDL_StartTextInput(); }
void platform_stop_text_input() { SDL_StopTextInput(); }
uint32_t platform_ticks() { return SDL_GetTicks(); }
void platform_delay(uint32_t milliseconds) { SDL_Delay(milliseconds); }

bool platform_decode_thumbnail_lvgl(const std::vector<uint8_t>& encoded, int width, int height,
                                    std::vector<uint8_t>& pixels, bool* has_alpha) {
    pixels.clear();
    if (encoded.empty() || width <= 0 || height <= 0 ||
        static_cast<uint64_t>(width) * height > 512U * 1024U || !bounded_thumbnail_dimensions(encoded)) return false;
    SDL_RWops* stream = SDL_RWFromConstMem(encoded.data(), static_cast<int>(encoded.size()));
    SDL_Surface* decoded = stream ? IMG_Load_RW(stream, 1) : nullptr;
    SDL_Surface* scaled = decoded ? SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888) : nullptr;
    if (decoded && scaled) {
        SDL_SetSurfaceBlendMode(decoded, SDL_BLENDMODE_NONE);
        if (SDL_BlitScaled(decoded, nullptr, scaled, nullptr) != 0) {
            SDL_FreeSurface(scaled);
            scaled = nullptr;
        }
    }
    if (decoded) SDL_FreeSurface(decoded);
    if (!scaled) return false;
    bool alpha = false;
    if (has_alpha) {
        for (int row = 0; row < height && !alpha; ++row) {
            const auto* source = reinterpret_cast<const uint32_t*>(
                static_cast<const uint8_t*>(scaled->pixels) + static_cast<size_t>(row) * scaled->pitch);
            for (int column = 0; column < width; ++column) {
                if ((source[column] >> 24) != 255U) { alpha = true; break; }
            }
        }
    }
    const size_t pixel_size = has_alpha && !alpha ? sizeof(lv_color_t) : LV_IMG_PX_SIZE_ALPHA_BYTE;
    pixels.resize(static_cast<size_t>(width) * height * pixel_size);
    for (int row = 0; row < height; ++row) {
#if LV_COLOR_DEPTH == 32
        std::memcpy(pixels.data() + static_cast<size_t>(row) * width * sizeof(uint32_t),
                    static_cast<uint8_t*>(scaled->pixels) + static_cast<size_t>(row) * scaled->pitch,
                    static_cast<size_t>(width) * sizeof(uint32_t));
#else
        const auto* source = reinterpret_cast<const uint32_t*>(
            static_cast<const uint8_t*>(scaled->pixels) + static_cast<size_t>(row) * scaled->pitch);
        for (int column = 0; column < width; ++column) {
            const uint32_t argb = source[column];
            const lv_color_t color = lv_color_make(static_cast<uint8_t>(argb >> 16),
                static_cast<uint8_t>(argb >> 8), static_cast<uint8_t>(argb));
            uint8_t* destination = pixels.data() +
                (static_cast<size_t>(row) * width + column) * pixel_size;
            std::memcpy(destination, &color, sizeof(color));
            if (pixel_size > sizeof(color)) destination[pixel_size - 1] = static_cast<uint8_t>(argb >> 24);
        }
#endif
    }
    SDL_FreeSurface(scaled);
    if (has_alpha) *has_alpha = alpha;
    return true;
}

void platform_font_init() {
    if (!lv_freetype_init(4, 4, 0)) return;
    chinese_font.name = "C:/Windows/Fonts/msyh.ttc";
    chinese_font.weight = 18;
    chinese_font.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&chinese_font);
}

void platform_font_shutdown() {
    if (chinese_font.font) lv_ft_font_destroy(chinese_font.font);
    chinese_font = {};
    lv_freetype_destroy();
}

const lv_font_t* platform_ui_font(const lv_font_t* fallback) {
    return chinese_font.font ? chinese_font.font : fallback;
}

std::string platform_config_path() {
    if (ui_audit_enabled()) {
        const std::string directory = ui_audit_directory();
        if (directory.empty()) return "NUL";
        CreateDirectoryA(directory.c_str(), nullptr);
        return directory + "\\wiliwili_config.json";
    }
    char local_app_data[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, MAX_PATH);
    const std::string base = length && length < MAX_PATH ? local_app_data : ".";
    const std::string directory = base + "\\xfangfang\\wiliwili_lvgl";
    CreateDirectoryA((base + "\\xfangfang").c_str(), nullptr);
    CreateDirectoryA(directory.c_str(), nullptr);
    return directory + "\\wiliwili_config.json";
}

void platform_set_http_proxy(const std::string& address, bool enabled) {
    http_proxy_address = utf8_to_wide(address);
    http_proxy_enabled = enabled && !http_proxy_address.empty();
}

void platform_set_http_timeouts(uint32_t connect_ms, uint32_t receive_ms) {
    http_connect_timeout_ms = connect_ms < 1000 ? 1000 : connect_ms;
    http_receive_timeout_ms = receive_ms < 1000 ? 1000 : receive_ms;
}

void platform_set_tls_verify(bool enabled) { tls_verify_enabled = enabled; }

void platform_set_gamepad_ab_swap(bool enabled) { gamepad_ab_swapped = enabled; }

void platform_open_external_url(const std::string& url) {
    if (ui_audit_enabled()) {
        std::ofstream log(ui_audit_directory() + "/external-url.log", std::ios::app);
        log << url << '\n';
        return;
    }
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

bool platform_https_get(const std::string& url, const std::string& headers,
                        std::vector<uint8_t>& body, std::string& error,
                        std::string* response_headers, size_t max_body_bytes) {
    if (!ui_audit_fixture_directory().empty()) {
        if (response_headers) response_headers->clear();
        const bool success = ui_audit_fixture_get(url, body, error, max_body_bytes);
        if (success && response_headers &&
            url == "https://passport.bilibili.com/x/passport-login/web/qrcode/poll?qrcode_key=offline-test-key&source=main_electron_pc") {
            std::ifstream input(ui_audit_fixture_directory() + "/qr_poll_headers.txt", std::ios::binary | std::ios::ate);
            if (input && input.tellg() >= 0 && input.tellg() <= 4096) {
                response_headers->resize(static_cast<size_t>(input.tellg()));
                input.seekg(0);
                if (!response_headers->empty()) input.read(response_headers->data(), response_headers->size());
            }
        }
        return success;
    }
    constexpr char kPrefix[] = "https://";
    if (url.rfind(kPrefix, 0) != 0) {
        error = "Only HTTPS URLs are supported";
        return false;
    }
    const size_t host_start = sizeof(kPrefix) - 1;
    const size_t host_end = url.find('/', host_start);
    const std::string host_text = url.substr(host_start, host_end - host_start);
    const std::string path_text = host_end == std::string::npos ? "/" : url.substr(host_end);
    const std::wstring host = utf8_to_wide(host_text);
    const std::wstring path = utf8_to_wide(path_text);
    const std::wstring request_headers = utf8_to_wide(headers);
    if (host.empty() || path.empty()) {
        error = "Invalid HTTPS URL";
        return false;
    }

    HINTERNET session = open_https_session();
    if (!session) {
        error = "Windows HTTPS initialization failed";
        return false;
    }
    HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr,
                                                     WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                     WINHTTP_FLAG_SECURE) : nullptr;
    const bool received = request && configure_tls(request) &&
        WinHttpSendRequest(request, request_headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : request_headers.c_str(),
                           request_headers.empty() ? 0 : -1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr);
    if (!received) {
        error = "HTTPS request failed";
        if (request) WinHttpCloseHandle(request);
        if (connect) WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }
    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size,
                             WINHTTP_NO_HEADER_INDEX) || status_code != 200) {
        error = "HTTPS HTTP " + std::to_string(status_code);
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }
    if (response_headers) {
        DWORD header_size = 0;
        WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                            WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &header_size,
                            WINHTTP_NO_HEADER_INDEX);
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && header_size > sizeof(wchar_t)) {
            std::vector<wchar_t> raw(header_size / sizeof(wchar_t) + 1);
            if (WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                    WINHTTP_HEADER_NAME_BY_INDEX, raw.data(), &header_size,
                                    WINHTTP_NO_HEADER_INDEX)) {
                *response_headers = wide_to_utf8(raw.data());
            }
        }
    }
    const bool read_ok = read_bounded_http_body(
        [request](size_t& available) {
            DWORD count = 0;
            const bool ok = WinHttpQueryDataAvailable(request, &count) != FALSE;
            available = count;
            return ok;
        },
        [request](uint8_t* destination, size_t requested, size_t& received) {
            DWORD count = 0;
            const bool ok = WinHttpReadData(request, destination, static_cast<DWORD>(requested), &count) != FALSE;
            received = count;
            return ok;
        }, body, max_body_bytes, error);
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    if (!read_ok || body.empty()) {
        if (error.empty()) error = read_ok ? "HTTPS response was empty" : "HTTPS response read failed";
        return false;
    }
    return true;
}

bool platform_https_post(const std::string& url, const std::string& headers,
                         const std::string& form_body, std::vector<uint8_t>& body,
                         std::string& error) {
    if (ui_audit_enabled()) {
        error = "Background audit blocks account write requests";
        return false;
    }
    constexpr char kPrefix[] = "https://";
    if (url.rfind(kPrefix, 0) != 0) {
        error = "Only HTTPS URLs are supported";
        return false;
    }
    const size_t host_start = sizeof(kPrefix) - 1;
    const size_t host_end = url.find('/', host_start);
    const std::string host_text = url.substr(host_start, host_end - host_start);
    const std::string path_text = host_end == std::string::npos ? "/" : url.substr(host_end);
    const std::wstring host = utf8_to_wide(host_text);
    const std::wstring path = utf8_to_wide(path_text);
    std::wstring request_headers = utf8_to_wide(headers);
    if (request_headers.find(L"Content-Type:") == std::wstring::npos)
        request_headers += L"Content-Type: application/x-www-form-urlencoded\r\n";
    if (host.empty() || path.empty()) {
        error = "Invalid HTTPS URL";
        return false;
    }
    HINTERNET session = open_https_session();
    HINTERNET connect = session ? WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0) : nullptr;
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"POST", path.c_str(), nullptr,
                                                     WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                     WINHTTP_FLAG_SECURE) : nullptr;
    const bool received = request && configure_tls(request) &&
        WinHttpSendRequest(request, request_headers.c_str(), static_cast<DWORD>(-1L),
                           const_cast<char*>(form_body.data()), static_cast<DWORD>(form_body.size()),
                           static_cast<DWORD>(form_body.size()), 0) &&
        WinHttpReceiveResponse(request, nullptr);
    if (!received) {
        error = "HTTPS POST failed";
        if (request) WinHttpCloseHandle(request);
        if (connect) WinHttpCloseHandle(connect);
        if (session) WinHttpCloseHandle(session);
        return false;
    }
    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size,
                             WINHTTP_NO_HEADER_INDEX) || status_code < 200 || status_code >= 300) {
        error = "HTTPS POST HTTP " + std::to_string(status_code);
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }
    constexpr size_t kMaxPostResponseBytes = 512U * 1024U;
    const bool read_ok = read_bounded_http_body(
        [request](size_t& available) {
            DWORD count = 0;
            const bool ok = WinHttpQueryDataAvailable(request, &count) != FALSE;
            available = count;
            return ok;
        },
        [request](uint8_t* destination, size_t requested, size_t& received) {
            DWORD count = 0;
            const bool ok = WinHttpReadData(request, destination, static_cast<DWORD>(requested), &count) != FALSE;
            received = count;
            return ok;
        }, body, kMaxPostResponseBytes, error);
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return read_ok;
}

bool platform_md5_hex(const std::string& source, std::string& digest, std::string& error) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0;
    DWORD bytes = 0;
    std::vector<unsigned char> object;
    std::vector<unsigned char> output(16);
    const NTSTATUS open_status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_MD5_ALGORITHM, nullptr, 0);
    if (open_status < 0 || BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                                             reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
                                             &bytes, 0) < 0) {
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
        error = "Windows MD5 initialization failed";
        return false;
    }
    object.resize(object_size);
    const NTSTATUS create_status = BCryptCreateHash(algorithm, &hash, object.data(), object_size,
                                                    nullptr, 0, 0);
    const NTSTATUS data_status = create_status >= 0 ? BCryptHashData(
        hash, reinterpret_cast<PUCHAR>(const_cast<char*>(source.data())), static_cast<ULONG>(source.size()), 0) : -1;
    const NTSTATUS finish_status = data_status >= 0 ? BCryptFinishHash(hash, output.data(),
                                                                        static_cast<ULONG>(output.size()), 0) : -1;
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (finish_status < 0) {
        error = "Windows MD5 calculation failed";
        return false;
    }
    static constexpr char hex[] = "0123456789abcdef";
    digest.clear();
    digest.reserve(output.size() * 2);
    for (unsigned char value : output) {
        digest += hex[value >> 4];
        digest += hex[value & 0x0f];
    }
    return true;
}

bool platform_player_prepare(const PlatformPlayerSpec& spec, std::string& error) {
    if (!spec.separate_video_plane) {
        error = "Windows preview requires separate-plane playback";
        return false;
    }
    h264_test_source = utf8_to_wide(spec.stream_uri);
    h264_test_audio_source = utf8_to_wide(spec.audio_stream_uri);
    h264_test_raw_annexb = spec.input == PlatformVideoInput::H264AnnexB;
    h264_test_seek_ms = 0;
    h264_test_volume = spec.volume_percent < 0 ? 0 : spec.volume_percent > 100 ? 100 : spec.volume_percent;
    h264_test_speed = spec.playback_speed_percent < 50 ? 50 :
                      spec.playback_speed_percent > 200 ? 200 : spec.playback_speed_percent;
    h264_test_aspect = spec.aspect == "4:3" || spec.aspect == "16:9" ? utf8_to_wide(spec.aspect) : L"";
    if (h264_test_source.empty()) {
        error = h264_test_raw_annexb ? "Windows preview has no H264 Annex-B source configured"
                                     : "Windows preview has no container stream configured";
        return false;
    }
    return true;
}

bool platform_player_start(std::string& error) {
    if (ui_audit_enabled()) {
        error = "Background audit blocks external playback";
        return false;
    }
    if (h264_test_source.empty()) {
        error = h264_test_raw_annexb ? "Windows preview has no H264 Annex-B source configured"
                                     : "Windows preview has no container stream configured";
        return false;
    }
    if (h264_test_player.hProcess) {
        if (h264_test_paused && suspend_h264_process(false)) h264_test_paused = false;
        return true;
    }
    const std::wstring aspect_option = h264_test_aspect.empty() ? std::wstring{} : L" -aspect " + h264_test_aspect;
    const std::wstring input_option = h264_test_raw_annexb ? L" -f h264" : L"";
    std::wstring command;
    if (h264_test_raw_annexb) {
        command = L"\"C:\\msys64\\mingw64\\bin\\ffplay.exe\" -loglevel warning -protocol_whitelist file,http,https,tcp,tls,crypto -headers \"Referer: https://www.bilibili.com/\\r\\nUser-Agent: Mozilla/5.0\\r\\n\" -fflags nobuffer "
                  L"-flags low_delay -volume " + std::to_wstring(h264_test_volume) +
                  L" -vf setpts=PTS*" + std::to_wstring(100) + L"/" + std::to_wstring(h264_test_speed) +
                  L" -ss " + std::to_wstring(h264_test_seek_ms / 1000) +
                  aspect_option + input_option + L" -i \"" + h264_test_source + L"\"";
    } else {
        const std::wstring audio_option = h264_test_audio_source.empty() ? std::wstring{} :
            L" --audio-file=\"" + h264_test_audio_source + L"\"";
        const std::wstring aspect = h264_test_aspect.empty() ? std::wstring{} :
            L" --video-aspect-override=" + h264_test_aspect;
        const int speed_fraction = h264_test_speed % 100;
        const std::wstring mpv_speed = std::to_wstring(h264_test_speed / 100) + L"." +
            (speed_fraction < 10 ? L"0" : L"") + std::to_wstring(speed_fraction);
        command = L"\"C:\\msys64\\mingw64\\bin\\mpv.exe\" --no-config --force-window=yes --terminal=no "
                  L"--http-header-fields=Referer:https://www.bilibili.com/,User-Agent:Mozilla/5.0 "
                  L"--volume=" + std::to_wstring(h264_test_volume) +
                  L" --speed=" + mpv_speed +
                  L" --start=" + std::to_wstring(h264_test_seek_ms / 1000) + aspect + audio_option +
                  L" \"" + h264_test_source + L"\"";
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                        &startup, &h264_test_player)) {
        error = "Windows H264 Annex-B test decoder could not start";
        return false;
    }
    return true;
}

bool platform_player_pause(std::string& error) {
    if (!h264_test_player.hProcess) return true;
    if (!h264_test_paused && suspend_h264_process(true)) {
        h264_test_paused = true;
        return true;
    }
    error = "Windows H264 Annex-B pause failed";
    return false;
}

bool platform_player_seek(uint32_t position_ms, std::string& error) {
    h264_test_seek_ms = position_ms;
    if (!h264_test_player.hProcess) return true;
    platform_player_stop();
    return platform_player_start(error);
}

bool platform_player_set_volume(uint8_t percent, std::string& error) {
    h264_test_volume = percent;
    if (!h264_test_player.hProcess) return true;
    // ffplay has no stable remote volume API. Restart the explicit test-only
    // raw-H264 process so its command-line volume matches the LVGL control.
    platform_player_stop();
    return platform_player_start(error);
}

bool platform_player_set_speed(uint16_t percent, std::string& error) {
    h264_test_speed = percent < 50 ? 50 : percent > 200 ? 200 : percent;
    if (!h264_test_player.hProcess) return true;
    platform_player_stop();
    return platform_player_start(error);
}

bool platform_player_finished() {
    return h264_test_player.hProcess && WaitForSingleObject(h264_test_player.hProcess, 0) == WAIT_OBJECT_0;
}

void platform_player_stop() {
    if (!h264_test_player.hProcess) return;
    TerminateProcess(h264_test_player.hProcess, 0);
    CloseHandle(h264_test_player.hThread);
    CloseHandle(h264_test_player.hProcess);
    h264_test_player = {};
    h264_test_paused = false;
}
