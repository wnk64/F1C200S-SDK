#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_image.h>
#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>

extern "C" {
#include "lvgl.h"
#include "extra/libs/freetype/lv_freetype.h"
}

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr wchar_t kRecommendPath[] =
    L"/x/web-interface/wbi/index/top/feed/rcmd?fresh_idx=1&ps=8&feed_version=V1&fresh_type=0&plat=1&x_num=3&y_num=4";

struct Video {
    std::string bvid;
    std::string title;
    std::string owner;
    std::string view;
    std::string pic;
    std::vector<uint8_t> image_bytes;
};

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_Texture* texture = nullptr;
std::vector<uint32_t> frame(kWindowWidth * kWindowHeight);
lv_indev_data_t pointer_data{};
lv_ft_info_t chinese_font{};
std::mutex data_mutex;
std::vector<Video> pending_videos;
std::string pending_error;
std::atomic_bool fetch_complete{false};
std::atomic_bool fetch_in_flight{false};
lv_obj_t* content = nullptr;
lv_obj_t* status_label = nullptr;
std::vector<std::vector<uint8_t>> thumbnail_pixels;
std::vector<lv_img_dsc_t> thumbnail_descriptors;

void flush(lv_disp_drv_t* driver, const lv_area_t* area, lv_color_t* colors) {
    for (int y = area->y1; y <= area->y2; ++y) {
        uint32_t* out = frame.data() + y * kWindowWidth + area->x1;
        for (int x = area->x1; x <= area->x2; ++x) {
            *out++ = colors->full;
            ++colors;
        }
    }
    SDL_UpdateTexture(texture, nullptr, frame.data(), kWindowWidth * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
    lv_disp_flush_ready(driver);
}

void read_pointer(lv_indev_drv_t*, lv_indev_data_t* data) {
    *data = pointer_data;
}

const lv_font_t* ui_font() {
    return chinese_font.font ? chinese_font.font : &lv_font_montserrat_16;
}

void set_label(lv_obj_t* parent, const char* text, int size, lv_color_t color,
               lv_align_t align, int x, int y) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, ui_font(), 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    lv_obj_set_style_text_line_space(label, 4, 0);
    lv_obj_align(label, align, x, y);
    (void)size;
}

std::string decode_json_string(const std::string& encoded) {
    std::string decoded;
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] != '\\' || i + 1 >= encoded.size()) {
            decoded += encoded[i];
            continue;
        }
        const char next = encoded[++i];
        switch (next) {
            case '"': decoded += '"'; break;
            case '\\': decoded += '\\'; break;
            case '/': decoded += '/'; break;
            case 'n': decoded += '\n'; break;
            case 'r': decoded += '\r'; break;
            case 't': decoded += '\t'; break;
            default: decoded += next; break;
        }
    }
    return decoded;
}

bool json_string_at(const std::string& object, const char* key, std::string& value) {
    const std::string prefix = std::string("\"") + key + "\":\"";
    const size_t begin = object.find(prefix);
    if (begin == std::string::npos) return false;
    std::string raw;
    for (size_t i = begin + prefix.size(); i < object.size(); ++i) {
        if (object[i] == '"' && (i == 0 || object[i - 1] != '\\')) {
            value = decode_json_string(raw);
            return true;
        }
        raw += object[i];
    }
    return false;
}

bool json_number_at(const std::string& object, const char* key, std::string& value) {
    const std::string prefix = std::string("\"") + key + "\":";
    const size_t begin = object.find(prefix);
    if (begin == std::string::npos) return false;
    const size_t start = begin + prefix.size();
    size_t end = start;
    while (end < object.size() && object[end] >= '0' && object[end] <= '9') ++end;
    if (end == start) return false;
    value = object.substr(start, end - start);
    return true;
}

std::vector<Video> parse_recommend(const std::string& body) {
    std::vector<Video> videos;
    size_t cursor = 0;
    while (videos.size() < 8) {
        const size_t marker = body.find("\"bvid\":\"", cursor);
        if (marker == std::string::npos) break;
        const size_t start = body.rfind('{', marker);
        const size_t end = body.find("\"track_id\":", marker);
        if (start == std::string::npos || end == std::string::npos) break;
        const std::string object = body.substr(start, end - start);
        Video video;
        std::string owner_object;
        json_string_at(object, "bvid", video.bvid);
        json_string_at(object, "title", video.title);
        json_string_at(object, "pic", video.pic);
        const size_t owner_pos = object.find("\"owner\":{");
        if (owner_pos != std::string::npos) {
            owner_object = object.substr(owner_pos);
            json_string_at(owner_object, "name", video.owner);
        }
        const size_t stat_pos = object.find("\"stat\":{");
        if (stat_pos != std::string::npos) json_number_at(object.substr(stat_pos), "view", video.view);
        if (!video.bvid.empty() && !video.title.empty()) videos.push_back(std::move(video));
        cursor = end;
    }
    return videos;
}

bool fetch_recommend(std::string& response, std::string& error) {
    HINTERNET session = WinHttpOpen(L"wiliwili-lvgl/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { error = "WinHTTP initialization failed"; return false; }
    HINTERNET connect = WinHttpConnect(session, L"api.bilibili.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) { WinHttpCloseHandle(session); error = "Bilibili connection failed"; return false; }
    HINTERNET request = WinHttpOpenRequest(connect, L"GET", kRecommendPath, nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request || !WinHttpSendRequest(request, L"Referer: https://www.bilibili.com/\r\n", -1L,
                                        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, nullptr)) {
        if (request) WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        error = "Bilibili request failed";
        return false;
    }
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
        std::vector<char> buffer(available + 1);
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), available, &read)) break;
        response.append(buffer.data(), read);
    }
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    if (response.empty()) { error = "Bilibili returned no data"; return false; }
    return true;
}

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    std::wstring wide(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wide.data(), length);
    wide.pop_back();
    return wide;
}

bool fetch_thumbnail(const std::string& url, std::vector<uint8_t>& bytes) {
    constexpr const char kHttpPrefix[] = "http://";
    constexpr const char kHttpsPrefix[] = "https://";
    std::string address = url;
    if (address.rfind(kHttpPrefix, 0) == 0) address.replace(0, sizeof(kHttpPrefix) - 1, kHttpsPrefix);
    if (address.rfind(kHttpsPrefix, 0) != 0) return false;
    const size_t host_end = address.find('/', sizeof(kHttpsPrefix) - 1);
    if (host_end == std::string::npos) return false;
    const std::wstring host = utf8_to_wide(address.substr(sizeof(kHttpsPrefix) - 1, host_end - (sizeof(kHttpsPrefix) - 1)));
    const std::wstring path = utf8_to_wide(address.substr(host_end));
    HINTERNET session = WinHttpOpen(L"wiliwili-lvgl/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return false;
    HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    bool success = request && WinHttpSendRequest(request, L"Referer: https://www.bilibili.com/\r\n", -1L,
                                                  WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                   WinHttpReceiveResponse(request, nullptr);
    DWORD available = 0;
    while (success && WinHttpQueryDataAvailable(request, &available) && available > 0) {
        const size_t offset = bytes.size();
        bytes.resize(offset + available);
        DWORD read = 0;
        if (!WinHttpReadData(request, bytes.data() + offset, available, &read)) { success = false; break; }
        bytes.resize(offset + read);
    }
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return success && !bytes.empty();
}

void prepare_thumbnails(const std::vector<Video>& videos) {
    thumbnail_pixels.clear();
    thumbnail_descriptors.clear();
    thumbnail_pixels.resize(videos.size());
    thumbnail_descriptors.resize(videos.size());
    for (size_t i = 0; i < videos.size(); ++i) {
        if (videos[i].image_bytes.empty()) continue;
        SDL_RWops* stream = SDL_RWFromConstMem(videos[i].image_bytes.data(), static_cast<int>(videos[i].image_bytes.size()));
        SDL_Surface* decoded = stream ? IMG_Load_RW(stream, 1) : nullptr;
        SDL_Surface* scaled = decoded ? SDL_CreateRGBSurfaceWithFormat(0, 186, 104, 32, SDL_PIXELFORMAT_ARGB8888) : nullptr;
        if (decoded && scaled) SDL_BlitScaled(decoded, nullptr, scaled, nullptr);
        if (decoded) SDL_FreeSurface(decoded);
        if (!scaled) continue;
        thumbnail_pixels[i].resize(186 * 104 * sizeof(uint32_t));
        for (int row = 0; row < 104; ++row) {
            std::memcpy(thumbnail_pixels[i].data() + row * 186 * sizeof(uint32_t),
                        static_cast<uint8_t*>(scaled->pixels) + row * scaled->pitch, 186 * sizeof(uint32_t));
        }
        SDL_FreeSurface(scaled);
        lv_img_dsc_t& descriptor = thumbnail_descriptors[i];
        descriptor.header.always_zero = 0;
        descriptor.header.w = 186;
        descriptor.header.h = 104;
        descriptor.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
        descriptor.data_size = static_cast<uint32_t>(thumbnail_pixels[i].size());
        descriptor.data = thumbnail_pixels[i].data();
    }
}

void show_recommend_page(const std::vector<Video>& videos, const std::string& error);
void start_fetch();

void refresh_recommend(lv_event_t*) {
    if (fetch_in_flight.exchange(true)) return;
    if (content) {
        lv_obj_del(content);
        content = nullptr;
    }
    lv_label_set_text(status_label, "Refreshing Bilibili Home Recommend...");
    start_fetch();
}

void open_original_page(lv_event_t* event) {
    const auto* video = static_cast<const Video*>(lv_event_get_user_data(event));
    const std::string url = std::string("https://www.bilibili.com/video/") + video->bvid;
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void return_to_recommend(lv_event_t*) {
    show_recommend_page(pending_videos, pending_error);
}

void show_video_detail(const Video& video) {
    if (content) lv_obj_del(content);
    content = lv_obj_create(lv_scr_act());
    lv_obj_set_size(content, 1160, 510);
    lv_obj_set_pos(content, 60, 178);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    lv_label_set_text(status_label, "Video detail / source contract: video_activity.xml and PlayerActivity");
    lv_obj_t* player = lv_obj_create(content);
    lv_obj_set_size(player, 706, 397);
    lv_obj_set_pos(player, 0, 34);
    lv_obj_set_style_bg_color(player, lv_color_hex(0x05070b), 0);
    lv_obj_set_style_border_width(player, 0, 0);
    lv_obj_set_style_radius(player, 6, 0);
    const size_t video_index = static_cast<size_t>(&video - pending_videos.data());
    if (video_index < thumbnail_descriptors.size() && thumbnail_descriptors[video_index].data) {
        lv_obj_t* cover = lv_img_create(player);
        lv_img_set_src(cover, &thumbnail_descriptors[video_index]);
        lv_obj_set_size(cover, 558, 312);
        lv_obj_center(cover);
        lv_obj_clear_flag(cover, LV_OBJ_FLAG_CLICKABLE);
    }
    set_label(player, "Player backend migration pending", 16, lv_color_hex(0xaeb8c6), LV_ALIGN_CENTER, 0, -16);
    set_label(player, video.bvid.c_str(), 16, lv_color_hex(0x58b9ee), LV_ALIGN_CENTER, 0, 18);

    lv_obj_t* info = lv_obj_create(content);
    lv_obj_set_size(info, 420, 435);
    lv_obj_set_pos(info, 740, 34);
    lv_obj_set_style_bg_color(info, lv_color_hex(0x202735), 0);
    lv_obj_set_style_radius(info, 8, 0);
    lv_obj_set_style_border_width(info, 0, 0);
    lv_obj_set_style_pad_all(info, 24, 0);

    lv_obj_t* title = lv_label_create(info);
    lv_label_set_text(title, video.title.c_str());
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(title, 370);
    lv_obj_set_style_text_font(title, ui_font(), 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xf3f5f8), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    const std::string metadata = "UP: " + video.owner + "\nViews: " + video.view + "\nBV: " + video.bvid;
    lv_obj_t* meta = lv_label_create(info);
    lv_label_set_text(meta, metadata.c_str());
    lv_label_set_long_mode(meta, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(meta, 370);
    lv_obj_set_style_text_font(meta, ui_font(), 0);
    lv_obj_set_style_text_color(meta, lv_color_hex(0xaeb8c6), 0);
    lv_obj_align(meta, LV_ALIGN_TOP_LEFT, 0, 126);

    lv_obj_t* original = lv_btn_create(info);
    lv_obj_set_size(original, 370, 50);
    lv_obj_align(original, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_set_style_bg_color(original, lv_color_hex(0x2877a9), 0);
    lv_obj_set_style_radius(original, 6, 0);
    lv_obj_add_event_cb(original, open_original_page, LV_EVENT_CLICKED, const_cast<Video*>(&video));
    set_label(original, "Open original Bilibili page", 16, lv_color_hex(0xffffff), LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* back = lv_btn_create(info);
    lv_obj_set_size(back, 370, 42);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x374152), 0);
    lv_obj_set_style_radius(back, 6, 0);
    lv_obj_add_event_cb(back, return_to_recommend, LV_EVENT_CLICKED, nullptr);
    set_label(back, "Back to Recommend", 16, lv_color_hex(0xffffff), LV_ALIGN_CENTER, 0, 0);
}

void open_video(lv_event_t* event) {
    const auto* video = static_cast<const Video*>(lv_event_get_user_data(event));
    show_video_detail(*video);
}

void build_video_card(lv_obj_t* parent, const Video& video, int index) {
    lv_obj_t* card = lv_btn_create(parent);
    lv_obj_set_size(card, 548, 132);
    lv_obj_set_style_bg_color(card, lv_color_hex(index % 2 ? 0x252b38 : 0x202735), 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_add_event_cb(card, open_video, LV_EVENT_CLICKED, const_cast<Video*>(&video));

    lv_obj_t* preview = lv_obj_create(card);
    lv_obj_set_size(preview, 186, 104);
    lv_obj_align(preview, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_bg_color(preview, lv_color_hex(index % 2 ? 0x3c5d7f : 0x734c73), 0);
    lv_obj_set_style_radius(preview, 6, 0);
    lv_obj_set_style_border_width(preview, 0, 0);
    lv_obj_clear_flag(preview, LV_OBJ_FLAG_CLICKABLE);
    if (static_cast<size_t>(index) < thumbnail_descriptors.size() && thumbnail_descriptors[index].data) {
        lv_obj_t* image = lv_img_create(preview);
        lv_img_set_src(image, &thumbnail_descriptors[index]);
        lv_obj_center(image);
        lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
    } else {
        set_label(preview, video.bvid.c_str(), 16, lv_color_hex(0xffffff), LV_ALIGN_CENTER, 0, 0);
    }

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, video.title.c_str());
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title, 320);
    lv_obj_set_style_text_font(title, ui_font(), 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xf3f5f8), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 212, 18);

    const std::string meta = video.owner + "   " + video.view + " views";
    lv_obj_t* detail = lv_label_create(card);
    lv_label_set_text(detail, meta.c_str());
    lv_obj_set_width(detail, 320);
    lv_obj_set_style_text_font(detail, ui_font(), 0);
    lv_obj_set_style_text_color(detail, lv_color_hex(0xaeb8c6), 0);
    lv_obj_align(detail, LV_ALIGN_BOTTOM_LEFT, 212, -18);
}

void show_recommend_page(const std::vector<Video>& videos, const std::string& error) {
    if (content) lv_obj_del(content);
    content = lv_obj_create(lv_scr_act());
    lv_obj_set_size(content, 1160, 510);
    lv_obj_set_pos(content, 60, 178);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    if (!error.empty()) {
        lv_label_set_text(status_label, error.c_str());
        return;
    }
    lv_label_set_text(status_label, "Bilibili Home Recommend - select a card for its in-app detail page");
    for (size_t i = 0; i < videos.size(); ++i) build_video_card(content, videos[i], static_cast<int>(i));
}

void build_shell() {
    lv_obj_t* root = lv_scr_act();
    lv_obj_set_style_bg_color(root, lv_color_hex(0x151922), 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_t* top = lv_obj_create(root);
    lv_obj_set_size(top, kWindowWidth, 126);
    lv_obj_set_style_bg_color(top, lv_color_hex(0x202633), 0);
    lv_obj_set_style_border_width(top, 0, 0);
    lv_obj_set_style_radius(top, 0, 0);
    set_label(top, "wiliwili", 24, lv_color_hex(0xffffff), LV_ALIGN_LEFT_MID, 54, -20);
    set_label(top, "Home    Dynamic    Mine", 16, lv_color_hex(0xe9edf2), LV_ALIGN_CENTER, 0, -20);
    set_label(top, "Live     Recommend     Popular     Bangumi     Cinema", 16,
              lv_color_hex(0x58b9ee), LV_ALIGN_CENTER, 0, 22);
    lv_obj_t* refresh = lv_btn_create(top);
    lv_obj_set_size(refresh, 92, 38);
    lv_obj_align(refresh, LV_ALIGN_RIGHT_MID, -42, 0);
    lv_obj_set_style_bg_color(refresh, lv_color_hex(0x2877a9), 0);
    lv_obj_set_style_radius(refresh, 6, 0);
    lv_obj_add_event_cb(refresh, refresh_recommend, LV_EVENT_CLICKED, nullptr);
    set_label(refresh, "Refresh", 16, lv_color_hex(0xffffff), LV_ALIGN_CENTER, 0, 0);
    status_label = lv_label_create(root);
    lv_obj_set_width(status_label, 1160);
    lv_obj_set_style_text_font(status_label, ui_font(), 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xb8c3cf), 0);
    lv_obj_set_pos(status_label, 60, 142);
    lv_label_set_text(status_label, "Loading the real Bilibili Home Recommend feed...");
}

void start_fetch() {
    std::thread([] {
        std::string body;
        std::string error;
        std::vector<Video> videos;
        if (fetch_recommend(body, error)) {
            videos = parse_recommend(body);
            if (videos.empty()) error = "Bilibili response could not be parsed";
            for (auto& video : videos) fetch_thumbnail(video.pic, video.image_bytes);
        }
        std::lock_guard<std::mutex> lock(data_mutex);
        pending_videos = std::move(videos);
        pending_error = std::move(error);
        fetch_in_flight = false;
        fetch_complete = true;
    }).detach();
}

}  // namespace

int main() {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) return 1;
    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_WEBP);
    window = SDL_CreateWindow("wiliwili LVGL", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              kWindowWidth, kWindowHeight, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                kWindowWidth, kWindowHeight);
    if (!window || !renderer || !texture) return 1;
    lv_init();
    if (lv_freetype_init(4, 4, 0)) {
        chinese_font.name = "C:/Windows/Fonts/msyh.ttc";
        chinese_font.weight = 18;
        chinese_font.style = FT_FONT_STYLE_NORMAL;
        lv_ft_font_init(&chinese_font);
    }
    std::vector<lv_color_t> draw_a(kWindowWidth * kWindowHeight);
    std::vector<lv_color_t> draw_b(kWindowWidth * kWindowHeight);
    lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, draw_a.data(), draw_b.data(), draw_a.size());
    lv_disp_drv_t display;
    lv_disp_drv_init(&display);
    display.hor_res = kWindowWidth;
    display.ver_res = kWindowHeight;
    display.draw_buf = &draw_buf;
    display.flush_cb = flush;
    lv_disp_drv_register(&display);
    lv_indev_drv_t pointer;
    lv_indev_drv_init(&pointer);
    pointer.type = LV_INDEV_TYPE_POINTER;
    pointer.read_cb = read_pointer;
    lv_indev_drv_register(&pointer);
    build_shell();
    start_fetch();

    bool running = true;
    bool displayed = false;
    uint32_t last_tick = SDL_GetTicks();
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) running = false;
            if (event.type == SDL_MOUSEMOTION) { pointer_data.point.x = event.motion.x; pointer_data.point.y = event.motion.y; }
            if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
                pointer_data.point.x = event.button.x;
                pointer_data.point.y = event.button.y;
                pointer_data.state = event.type == SDL_MOUSEBUTTONDOWN ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
            }
        }
        const uint32_t now = SDL_GetTicks();
        lv_tick_inc(now - last_tick);
        last_tick = now;
        if (fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            prepare_thumbnails(pending_videos);
            show_recommend_page(pending_videos, pending_error);
            displayed = true;
        }
        lv_timer_handler();
        SDL_Delay(5);
    }
    if (chinese_font.font) lv_ft_font_destroy(chinese_font.font);
    lv_freetype_destroy();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
