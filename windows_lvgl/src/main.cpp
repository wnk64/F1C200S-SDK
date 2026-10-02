extern "C" {
#include "lvgl.h"
#include "extra/libs/qrcode/lv_qrcode.h"
}

#include "platform_services.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <future>
#include <map>
#include <mutex>
#include <string>
#include "../../board_linux/compat_cpp17.h"
#include <thread>
#include <vector>

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kBoardWidth = 800;
constexpr int kBoardHeight = 480;
constexpr size_t kMaxFeedVideos = 32;
constexpr size_t kMaxVideoComments = 24;
constexpr size_t kMaxCommentAuthorBytes = 96;
constexpr size_t kMaxCommentMessageBytes = 1024;
constexpr size_t kMaxEncodedThumbnailBytes = 128U * 1024U;
constexpr size_t kEncodedThumbnailBatchBytes = 1024U * 1024U;
constexpr char kRecommendPath[] =
    "/x/web-interface/wbi/index/top/feed/rcmd?fresh_idx=1&ps=8&feed_version=V1&fresh_type=0&plat=1&x_num=3&y_num=4";
constexpr char kPopularPath[] = "/x/web-interface/popular?pn=1&ps=8";
constexpr char kBilibiliHeaders[] =
    "Referer: https://www.bilibili.com/\r\n"
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36\r\n"
    "Accept: application/json, text/plain, */*\r\n"
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8\r\n";
constexpr std::array<int, 64> kWbiMixinTable = {46, 47, 18, 2, 53, 8, 23, 32, 15, 50, 10, 31, 58, 3, 45, 35,
                                                 27, 43, 5, 49, 33, 9, 42, 19, 29, 28, 14, 39, 12, 38, 41, 13,
                                                 37, 48, 7, 16, 24, 55, 40, 61, 26, 17, 0, 1, 60, 51, 30, 4,
                                                 22, 25, 54, 21, 56, 59, 6, 63, 57, 62, 11, 36, 20, 34, 44, 52};

struct VideoPart {
    std::string cid;
    std::string duration;
    std::string title;
};

struct VideoComment {
    std::string author;
    std::string message;
};

struct CommentPageResult {
    std::vector<VideoComment> comments;
    std::string bvid;
    std::string error;
    size_t video_index{};
    uint32_t epoch{};
    int page{};
};

struct DanmakuItem {
    uint32_t time_ms{};
    std::string text;
};

struct RelatedVideo {
    std::string bvid;
    std::string title;
    std::string owner;
    std::string view;
};

struct HotTerm {
    std::string keyword;
    std::string heat;
};

struct MediaSearchItem {
    std::string id;
    std::string title;
    std::string subtitle;
};

struct SeasonEpisode {
    std::string bvid;
    std::string cid;
    std::string title;
    std::string badge;
};

struct SeasonDetail {
    std::string season_id;
    std::string title;
    std::string subtitle;
    std::string description;
    std::vector<SeasonEpisode> episodes;
};

struct DynamicItem {
    std::string id;
    std::string bvid;
    std::string title;
    std::string summary;
};

struct DynamicUp {
    std::string name;
    std::string mid;
};

struct InboxItem {
    std::string sender;
    std::string message;
    std::string talker_id;
    std::string session_type;
    std::string timestamp;
    std::string unread_count;
    std::string sequence;
};

struct Video {
    std::string aid;
    std::string bvid;
    std::string title;
    std::string owner;
    std::string view;
    std::string cid;
    std::string duration;
    std::string description;
    std::string comment_count;
    std::vector<VideoComment> comments;
    std::vector<DanmakuItem> danmaku;
    bool comments_exhausted{};
    std::vector<RelatedVideo> related;
    std::string play_url;
    std::string play_audio_url;
    std::string play_format;
    std::vector<VideoPart> parts;
    std::string pic;
    std::vector<uint8_t> image_bytes;
};

int window_width = kWindowWidth;
int window_height = kWindowHeight;
bool board_preview = false;
lv_indev_data_t pointer_data{};
lv_group_t* focus_group = nullptr;
std::mutex data_mutex;
std::vector<Video> pending_videos;
// LVGL keeps event callbacks after the feed response has been parsed. Keep the
// displayed model separate from the worker's pending result and pass indexes,
// never pointers into a vector that a later refresh can replace.
std::vector<Video> displayed_videos;
std::vector<std::string> search_history;
std::vector<HotTerm> displayed_hot_terms;
std::vector<HotTerm> pending_hot_terms;
std::vector<MediaSearchItem> displayed_media_results;
std::vector<MediaSearchItem> pending_media_results;
SeasonDetail displayed_season;
SeasonDetail pending_season;
std::vector<DynamicItem> displayed_dynamic_items;
std::vector<DynamicItem> pending_dynamic_items;
std::vector<DynamicUp> displayed_dynamic_ups;
std::vector<DynamicUp> pending_dynamic_ups;
std::vector<InboxItem> displayed_inbox_items;
std::vector<InboxItem> pending_inbox_items;
std::vector<InboxItem> displayed_chat_messages;
std::vector<InboxItem> pending_chat_messages;
bool pending_chat_append = false;
std::string pending_chat_next_seqno;
std::vector<Video> displayed_history_videos;
std::vector<Video> pending_history_videos;
std::vector<Video> displayed_favorite_videos;
std::vector<Video> pending_favorite_videos;
std::vector<Video> displayed_later_videos;
std::vector<Video> pending_later_videos;
std::vector<std::string> displayed_subscription_titles;
std::vector<std::string> pending_subscription_titles;
std::vector<std::string> displayed_bangumi_titles;
std::vector<std::string> pending_bangumi_titles;
std::vector<std::string> displayed_series_titles;
std::vector<std::string> pending_series_titles;
std::string pending_error;
Video pending_detail;
size_t pending_detail_index = 0;
size_t pending_detail_part_index = 0;
std::string pending_detail_error;
Video pending_detail_aux;
size_t pending_detail_aux_index = 0;
CommentPageResult pending_more_comments;
int comment_next_page = 2;
std::atomic_bool fetch_complete{false};
std::atomic_bool fetch_in_flight{false};
std::atomic_bool detail_fetch_complete{false};
std::atomic_bool detail_fetch_in_flight{false};
std::atomic_bool detail_aux_fetch_complete{false};
std::atomic<uint32_t> navigation_epoch{0};
uint32_t pending_detail_epoch = 0;
uint32_t pending_detail_aux_epoch = 0;
uint32_t pending_feed_epoch = 0;
uint32_t pending_hot_epoch = 0;
uint32_t pending_media_epoch = 0;
uint32_t pending_season_epoch = 0;
std::atomic_bool comments_more_fetch_complete{false};
std::atomic_bool comments_more_fetch_in_flight{false};
std::atomic_bool hot_fetch_complete{false};
std::atomic_bool hot_fetch_in_flight{false};
std::atomic_bool media_fetch_complete{false};
std::atomic_bool media_fetch_in_flight{false};
std::atomic_bool season_fetch_complete{false};
std::atomic_bool season_fetch_in_flight{false};
std::atomic_bool dynamic_fetch_complete{false};
std::atomic_bool dynamic_fetch_in_flight{false};
std::atomic_bool dynamic_up_fetch_complete{false};
std::atomic_bool dynamic_up_fetch_in_flight{false};
std::atomic_bool inbox_fetch_complete{false};
std::atomic_bool inbox_fetch_in_flight{false};
std::atomic_bool chat_fetch_complete{false};
std::atomic_bool chat_fetch_in_flight{false};
std::string active_chat_talker;
std::string active_chat_session_type;
std::string chat_next_seqno;
std::atomic_bool history_fetch_complete{false};
std::atomic_bool history_fetch_in_flight{false};
std::atomic_bool favorite_fetch_complete{false};
std::atomic_bool favorite_fetch_in_flight{false};
std::atomic_bool later_fetch_complete{false};
std::atomic_bool later_fetch_in_flight{false};
std::atomic_bool subscription_fetch_complete{false};
std::atomic_bool subscription_fetch_in_flight{false};
std::atomic_bool bangumi_fetch_complete{false};
std::atomic_bool bangumi_fetch_in_flight{false};
std::atomic_bool series_fetch_complete{false};
std::atomic_bool series_fetch_in_flight{false};
std::array<bool, 6> mine_fetch_requested{};
std::array<bool, 4> inbox_fetch_requested{};
bool dynamic_fetch_requested = false;
bool dynamic_up_fetch_requested = false;
std::atomic_bool action_result_complete{false};
std::atomic_bool refresh_chat_after_action{false};
std::atomic_bool refresh_detail_after_action{false};
std::atomic_bool refresh_favorites_after_action{false};
std::atomic_bool refresh_later_after_action{false};
std::string pending_action_result;
std::atomic_bool popular_selected{false};
int feed_page = 1;
bool feed_append = false;
bool pending_feed_append = false;
enum class FeedMode { Recommend, Popular, Search };
enum class MainSection { Home, Dynamic, Mine, Inbox, Settings };
enum class PlayerSidePanel { Comments, Parts, Related };
enum class TextInputTarget { Search, DetailComment, InboxMessage, HttpProxy };
FeedMode feed_mode = FeedMode::Recommend;
MainSection main_section = MainSection::Home;
PlayerSidePanel player_side_panel = PlayerSidePanel::Comments;
std::string search_term;
lv_obj_t* search_input = nullptr;
lv_obj_t* active_text_input = nullptr;
TextInputTarget active_text_target = TextInputTarget::Search;
bool search_focused = false;
lv_obj_t* content = nullptr;
lv_obj_t* status_label = nullptr;
lv_obj_t* fps_label = nullptr;
uint32_t fps_window_start = 0;
uint32_t fps_frame_count = 0;
lv_obj_t* recommend_tab = nullptr;
lv_obj_t* popular_tab = nullptr;
lv_obj_t* main_nav[5]{};
lv_obj_t* nav_rail = nullptr;
lv_obj_t* top_title = nullptr;
std::vector<lv_obj_t*> home_header_controls;
lv_obj_t* dynamic_video_tab = nullptr;
lv_obj_t* dynamic_article_tab = nullptr;
bool dynamic_showing_articles = false;
int dynamic_article_page = 1;
std::string dynamic_video_offset;
std::string selected_dynamic_mid;
bool pending_dynamic_append = false;
std::string pending_dynamic_next_offset;
uint32_t dynamic_request_epoch = 0;
uint32_t pending_dynamic_epoch = 0;
bool pending_dynamic_articles = false;
std::string displayed_dynamic_error;
std::string pending_dynamic_error;
unsigned int mine_active_tab = 0;
std::vector<lv_obj_t*> mine_tabs;
unsigned int settings_active_tab = 0;
bool setting_hwdec = true;
bool setting_history_report = true;
// Detail-first is a UI invariant.  Playback starts only from the detail
// screen's Play button, so legacy auto-play preferences are ignored.
bool setting_autoplay = false;
bool setting_autofullscreen = false;
bool setting_low_quality = false;
bool setting_tls_verify = true;
bool setting_http_proxy_enabled = false;
std::string setting_http_proxy;
bool setting_swap_abxy = false;
std::string setting_keymap = "xbox";
std::string setting_app_theme = "auto";
bool setting_show_fps = false;
int setting_limited_fps = 0;
bool setting_hide_bottom_bar = false;
bool setting_tv_osd = false;
bool setting_danmaku_on = true;
int setting_danmaku_speed = 100;
int setting_danmaku_area = 100;
int setting_danmaku_alpha = 80;
bool fullscreen_active = false;
// LVGL transform zoom uses 256 as 100%.  Board builds keep this at 100% so
// the fixed 800x480 hitboxes and RGB565 buffers remain deterministic.
int setting_ui_scale = 100;
int setting_video_quality = 16;
int setting_video_format = 4048;
int setting_video_codec = 7;
int setting_audio_quality = 30280;
int setting_memory_cache = 64;
int setting_image_threads = 4;
int setting_http_timeout_seconds = 15;
int setting_http_connect_timeout_seconds = 10;
int setting_volume = 70;
int setting_playback_speed = 100;
int setting_player_strategy = 0;
std::string setting_aspect = "default";
bool player_playing = false;
bool player_history_reported = false;
std::string player_backend_error;
std::string h264_test_uri;
std::string launch_bvid;
std::string launch_season_id;
lv_obj_t* player_seek_label = nullptr;
lv_obj_t* player_volume_label = nullptr;
std::string wbi_mixin_key;
std::time_t wbi_key_update_time = 0;
std::string bilibili_cookie;
std::string cookie_import_path;
std::string account_name;
std::string qr_login_url;
std::string qr_login_key;
std::atomic_bool qr_fetch_complete{false};
std::atomic_bool qr_fetch_in_flight{false};
std::atomic_bool qr_status_complete{false};
std::string pending_qr_status;
lv_obj_t* qr_code_widget = nullptr;
lv_obj_t* qr_status_label = nullptr;
std::array<std::string, 4> account_stats{};
std::atomic_bool account_fetch_complete{false};
std::atomic_bool account_fetch_in_flight{false};
std::atomic_bool danmaku_fetch_complete{false};
std::atomic_bool danmaku_fetch_in_flight{false};
std::vector<DanmakuItem> pending_danmaku;
size_t pending_danmaku_index = 0;
lv_obj_t* danmaku_label = nullptr;
std::array<lv_obj_t*, 3> danmaku_tracks{};
uint32_t player_started_tick = 0;
uint32_t player_elapsed_offset_ms = 0;
std::string player_session_bvid;
size_t player_session_part_index = 0;
lv_obj_t* detail_comment_input = nullptr;
lv_obj_t* inbox_message_input = nullptr;
lv_obj_t* proxy_input = nullptr;
unsigned int inbox_active_tab = 0;
bool detail_visible = false;
bool season_page_visible = false;
bool detail_return_to_season = false;
bool detail_return_to_dynamic = false;
bool player_page_visible = false;
size_t active_video_index = 0;
size_t active_part_index = 0;
bool open_player_after_first_feed = false;
bool open_detail_after_first_feed = false;
bool open_player_after_detail = false;
bool force_detail_view = false;
bool open_parts_after_detail = false;
bool open_related_after_detail = false;
bool open_search_hots_on_start = false;
bool open_search_history_on_start = false;
std::string media_search_category;
bool open_media_search_on_start = false;
bool open_qr_on_start = false;
std::vector<std::vector<uint8_t>> thumbnail_pixels;
std::vector<lv_img_dsc_t> thumbnail_descriptors;

std::string format_duration(const std::string& seconds) {
    if (seconds.empty()) return {};
    const unsigned long value = std::strtoul(seconds.c_str(), nullptr, 10);
    return std::to_string(value / 60) + ":" + (value % 60 < 10 ? "0" : "") +
           std::to_string(value % 60);
}

std::string lvgl_config_path() {
    return platform_config_path();
}

std::string cookie_path() { return lvgl_config_path() + ".cookie"; }

void load_cookie() {
    std::ifstream input(cookie_path(), std::ios::binary);
    std::getline(input, bilibili_cookie);
}

void save_cookie() {
    std::ofstream output(cookie_path(), std::ios::binary | std::ios::trunc);
    if (output) output << bilibili_cookie << '\n';
}

std::string bilibili_headers() {
    std::string headers = kBilibiliHeaders;
    if (!bilibili_cookie.empty()) headers += "Cookie: " + bilibili_cookie + "\r\n";
    return headers;
}

std::string cookie_field(const std::string& name) {
    size_t cursor = 0;
    while (cursor < bilibili_cookie.size()) {
        while (cursor < bilibili_cookie.size() && (bilibili_cookie[cursor] == ' ' || bilibili_cookie[cursor] == ';')) ++cursor;
        const size_t equal = bilibili_cookie.find('=', cursor);
        if (equal == std::string::npos) break;
        const size_t end = bilibili_cookie.find(';', equal + 1);
        if (bilibili_cookie.compare(cursor, equal - cursor, name) == 0)
            return bilibili_cookie.substr(equal + 1, end == std::string::npos ? std::string::npos : end - equal - 1);
        cursor = end == std::string::npos ? bilibili_cookie.size() : end + 1;
    }
    return {};
}

std::string xml_unescape(std::string text) {
    const std::pair<const char*, const char*> entities[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"},
        {"&quot;", "\""}, {"&apos;", "'"}};
    for (const auto& entity : entities) {
        size_t cursor = 0;
        while ((cursor = text.find(entity.first, cursor)) != std::string::npos) {
            text.replace(cursor, std::strlen(entity.first), entity.second);
            cursor += std::strlen(entity.second);
        }
    }
    return text;
}

bool read_bool_setting(const std::string& source, const char* key, bool fallback) {
    const std::string needle = std::string("\"") + key + "\":";
    const size_t start = source.find(needle);
    if (start == std::string::npos) return fallback;
    const size_t value = start + needle.size();
    if (source.compare(value, 4, "true") == 0) return true;
    if (source.compare(value, 5, "false") == 0) return false;
    return fallback;
}

int read_int_setting(const std::string& source, const char* key, int fallback) {
    const std::string needle = std::string("\"") + key + "\":";
    const size_t start = source.find(needle);
    if (start == std::string::npos) return fallback;
    const char* begin = source.c_str() + start + needle.size();
    char* end = nullptr;
    const long value = std::strtol(begin, &end, 10);
    return end == begin ? fallback : static_cast<int>(value);
}

std::string read_string_setting(const std::string& source, const char* key, const std::string& fallback) {
    const std::string needle = std::string("\"") + key + "\":\"";
    const size_t start = source.find(needle);
    if (start == std::string::npos) return fallback;
    const size_t value = start + needle.size();
    const size_t end = source.find('"', value);
    return end == std::string::npos ? fallback : source.substr(value, end - value);
}

void load_lvgl_config() {
    std::ifstream input(lvgl_config_path(), std::ios::binary);
    if (!input) return;
    const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    setting_hwdec = read_bool_setting(source, "PLAYER_HWDEC", setting_hwdec);
    setting_history_report = read_bool_setting(source, "HISTORY_REPORT", setting_history_report);
    setting_autoplay = false;
    setting_autofullscreen = read_bool_setting(source, "PLAYER_AUTO_FULLSCREEN", setting_autofullscreen);
    setting_low_quality = read_bool_setting(source, "PLAYER_LOW_QUALITY", setting_low_quality);
    if (source.find("\"SHOW_FPS\":") != std::string::npos)
        setting_show_fps = read_bool_setting(source, "SHOW_FPS", setting_show_fps);
    else
        setting_show_fps = !read_bool_setting(source, "HIDE_FPS", !setting_show_fps);
    setting_hide_bottom_bar = read_bool_setting(source, "HIDE_BOTTOM_BAR", setting_hide_bottom_bar);
    setting_limited_fps = read_int_setting(source, "LIMITED_FPS", setting_limited_fps);
    if (setting_limited_fps != 0 && setting_limited_fps != 30 && setting_limited_fps != 60 && setting_limited_fps != 120)
        setting_limited_fps = 0;
    setting_tv_osd = read_bool_setting(source, "PLAYER_OSD_TV_MODE", setting_tv_osd);
    setting_danmaku_on = read_bool_setting(source, "DANMAKU_ON", setting_danmaku_on);
    setting_danmaku_speed = wiliwili::clamp(read_int_setting(source, "DANMAKU_STYLE_SPEED", setting_danmaku_speed), 50, 200);
    setting_danmaku_area = read_int_setting(source, "DANMAKU_STYLE_AREA", setting_danmaku_area);
    if (setting_danmaku_area != 25 && setting_danmaku_area != 50 && setting_danmaku_area != 75 && setting_danmaku_area != 100)
        setting_danmaku_area = 100;
    setting_danmaku_alpha = wiliwili::clamp(read_int_setting(source, "DANMAKU_STYLE_ALPHA", setting_danmaku_alpha), 10, 100);
    setting_tls_verify = read_bool_setting(source, "TLS_VERIFY", setting_tls_verify);
    setting_http_proxy_enabled = read_bool_setting(source, "HTTP_PROXY_STATUS", setting_http_proxy_enabled);
    setting_http_proxy = read_string_setting(source, "HTTP_PROXY", setting_http_proxy);
    setting_swap_abxy = read_bool_setting(source, "APP_SWAP_ABXY", setting_swap_abxy);
    setting_keymap = read_string_setting(source, "KEYMAP", setting_keymap);
    if (setting_keymap != "xbox" && setting_keymap != "ps" && setting_keymap != "keyboard") setting_keymap = "xbox";
    setting_app_theme = read_string_setting(source, "APP_THEME", setting_app_theme);
    if (setting_app_theme != "auto" && setting_app_theme != "light" && setting_app_theme != "dark") setting_app_theme = "auto";
    const int parsed = read_int_setting(source, "UI_SCALE", setting_ui_scale);
    if (parsed == 75 || parsed == 100 || parsed == 125 || parsed == 150) setting_ui_scale = parsed;
    setting_video_quality = read_int_setting(source, "VIDEO_QUALITY", setting_video_quality);
    setting_video_format = read_int_setting(source, "VIDEO_FORMAT", setting_video_format);
    if (setting_video_format != 4048 && setting_video_format != 0) setting_video_format = 4048;
    setting_video_codec = read_int_setting(source, "VIDEO_CODEC", setting_video_codec);
    if (setting_video_codec != 7 && setting_video_codec != 12 && setting_video_codec != 13) setting_video_codec = 7;
    setting_audio_quality = read_int_setting(source, "AUDIO_QUALITY", setting_audio_quality);
    setting_memory_cache = read_int_setting(source, "PLAYER_INMEMORY_CACHE", setting_memory_cache);
    setting_image_threads = read_int_setting(source, "IMAGE_REQUEST_THREADS", setting_image_threads);
    if (setting_image_threads != 1 && setting_image_threads != 2 && setting_image_threads != 4 && setting_image_threads != 8)
        setting_image_threads = 4;
    setting_http_timeout_seconds = read_int_setting(source, "HTTP_TIMEOUT", setting_http_timeout_seconds);
    setting_http_connect_timeout_seconds = read_int_setting(source, "HTTP_CONNECTION_TIMEOUT", setting_http_connect_timeout_seconds);
    if (setting_http_timeout_seconds != 5 && setting_http_timeout_seconds != 10 && setting_http_timeout_seconds != 15 && setting_http_timeout_seconds != 30)
        setting_http_timeout_seconds = 15;
    if (setting_http_connect_timeout_seconds != 5 && setting_http_connect_timeout_seconds != 10 && setting_http_connect_timeout_seconds != 15 && setting_http_connect_timeout_seconds != 30)
        setting_http_connect_timeout_seconds = 10;
    setting_volume = wiliwili::clamp(read_int_setting(source, "PLAYER_VOLUME", setting_volume), 0, 100);
    setting_playback_speed = wiliwili::clamp(read_int_setting(source, "PLAYER_DEFAULT_SPEED", setting_playback_speed), 50, 200);
    setting_player_strategy = read_int_setting(source, "PLAYER_STRATEGY", setting_player_strategy);
    if (setting_player_strategy < 0 || setting_player_strategy > 3) setting_player_strategy = 0;
    setting_aspect = read_string_setting(source, "PLAYER_ASPECT", setting_aspect);
    if (setting_aspect != "default" && setting_aspect != "4:3" && setting_aspect != "16:9")
        setting_aspect = "default";
}

void save_lvgl_config() {
    std::ofstream output(lvgl_config_path(), std::ios::binary | std::ios::trunc);
    if (!output) return;
    output << "{\n"
           << "  \"PLAYER_HWDEC\":" << (setting_hwdec ? "true" : "false") << ",\n"
           << "  \"HISTORY_REPORT\":" << (setting_history_report ? "true" : "false") << ",\n"
           << "  \"PLAYER_AUTO_PLAY\":" << (setting_autoplay ? "true" : "false") << ",\n"
           << "  \"PLAYER_AUTO_FULLSCREEN\":" << (setting_autofullscreen ? "true" : "false") << ",\n"
           << "  \"PLAYER_LOW_QUALITY\":" << (setting_low_quality ? "true" : "false") << ",\n"
           << "  \"SHOW_FPS\":" << (setting_show_fps ? "true" : "false") << ",\n"
           << "  \"HIDE_FPS\":" << (setting_show_fps ? "false" : "true") << ",\n"
           << "  \"HIDE_BOTTOM_BAR\":" << (setting_hide_bottom_bar ? "true" : "false") << ",\n"
           << "  \"LIMITED_FPS\":" << setting_limited_fps << ",\n"
           << "  \"PLAYER_OSD_TV_MODE\":" << (setting_tv_osd ? "true" : "false") << ",\n"
           << "  \"DANMAKU_ON\":" << (setting_danmaku_on ? "true" : "false") << ",\n"
           << "  \"DANMAKU_STYLE_SPEED\":" << setting_danmaku_speed << ",\n"
           << "  \"DANMAKU_STYLE_AREA\":" << setting_danmaku_area << ",\n"
           << "  \"DANMAKU_STYLE_ALPHA\":" << setting_danmaku_alpha << ",\n"
           << "  \"TLS_VERIFY\":" << (setting_tls_verify ? "true" : "false") << ",\n"
           << "  \"HTTP_PROXY_STATUS\":" << (setting_http_proxy_enabled ? "true" : "false") << ",\n"
           << "  \"HTTP_PROXY\":\"" << setting_http_proxy << "\",\n"
           << "  \"APP_SWAP_ABXY\":" << (setting_swap_abxy ? "true" : "false") << ",\n"
           << "  \"KEYMAP\":\"" << setting_keymap << "\",\n"
           << "  \"APP_THEME\":\"" << setting_app_theme << "\",\n"
           << "  \"UI_SCALE\":" << setting_ui_scale << ",\n"
           << "  \"VIDEO_QUALITY\":" << setting_video_quality << ",\n"
           << "  \"VIDEO_FORMAT\":" << setting_video_format << ",\n"
           << "  \"VIDEO_CODEC\":" << setting_video_codec << ",\n"
           << "  \"AUDIO_QUALITY\":" << setting_audio_quality << ",\n"
           << "  \"PLAYER_INMEMORY_CACHE\":" << setting_memory_cache << ",\n"
           << "  \"IMAGE_REQUEST_THREADS\":" << setting_image_threads << ",\n"
           << "  \"HTTP_TIMEOUT\":" << setting_http_timeout_seconds << ",\n"
           << "  \"HTTP_CONNECTION_TIMEOUT\":" << setting_http_connect_timeout_seconds << ",\n"
           << "  \"PLAYER_VOLUME\":" << setting_volume << ",\n"
           << "  \"PLAYER_DEFAULT_SPEED\":" << setting_playback_speed << ",\n"
           << "  \"PLAYER_STRATEGY\":" << setting_player_strategy << ",\n"
           << "  \"PLAYER_ASPECT\":\"" << setting_aspect << "\"\n"
           << "}\n";
}

std::string search_history_path() { return lvgl_config_path() + ".history"; }

bool is_valid_utf8(const std::string& value) {
    for (size_t index = 0; index < value.size();) {
        const unsigned char lead = static_cast<unsigned char>(value[index]);
        if (lead <= 0x7f) { ++index; continue; }
        size_t continuation_count = 0;
        if (lead >= 0xc2 && lead <= 0xdf) continuation_count = 1;
        else if (lead >= 0xe0 && lead <= 0xef) continuation_count = 2;
        else if (lead >= 0xf0 && lead <= 0xf4) continuation_count = 3;
        else return false;
        if (index + continuation_count >= value.size()) return false;
        for (size_t offset = 1; offset <= continuation_count; ++offset) {
            if ((static_cast<unsigned char>(value[index + offset]) & 0xc0) != 0x80) return false;
        }
        index += continuation_count + 1;
    }
    return true;
}

void load_search_history() {
    std::ifstream input(search_history_path(), std::ios::binary);
    std::string item;
    while (std::getline(input, item)) {
        // A legacy ANSI/corrupt history line is unsafe for the FreeType label
        // pipeline and previously made the history page spin at 100% CPU.
        if (!item.empty() && is_valid_utf8(item) && search_history.size() < 50) search_history.push_back(item);
    }
}

void save_search_history() {
    std::ofstream output(search_history_path(), std::ios::binary | std::ios::trunc);
    for (const std::string& item : search_history) output << item << '\n';
}

void add_search_history(const std::string& query) {
    if (query.empty()) return;
    search_history.erase(std::remove(search_history.begin(), search_history.end(), query), search_history.end());
    if (search_history.size() == 50) search_history.erase(search_history.begin());
    search_history.push_back(query);
    save_search_history();
}

lv_color_t ui_color(uint32_t color) {
    if (setting_app_theme == "light") {
        switch (color) {
            case 0x05070b: color = 0xf8f9fb; break;
            case 0x080b10: color = 0xf0f2f5; break;
            case 0x101722: color = 0xe7ebf0; break;
            case 0x111827: color = 0xf8f9fb; break;
            case 0x151922: color = 0xf0f2f5; break;
            case 0x182231: case 0x1d2330: case 0x1f2937: color = 0xffffff; break;
            case 0x202633: case 0x202735: case 0x2b3444: color = 0xe7ebf0; break;
            case 0x203d55: color = 0xdbeafe; break;
            case 0x243349: case 0x283348: color = 0xe2e8f0; break;
            case 0x374152: color = 0xcfd6df; break;
            case 0x29384b: case 0x344256: case 0x566273: color = 0xdce3eb; break;
            case 0x4b3540: case 0x8d3b45: color = 0xf8d7da; break;
            case 0xffa8b7: color = 0x9f1239; break;
            case 0x58b9ee: case 0x63c4f4: color = 0x0369a1; break;
            case 0x8bc8e8: case 0x8fd3ff: color = 0x075985; break;
            case 0x7e8a9a: color = 0x677383; break;
            case 0x9dacbd: case 0xaeb8c6: case 0xb8c3cf: case 0xc4ceda: color = 0x556170; break;
            case 0xc8d4df: case 0xdce8f3: color = 0x334155; break;
            case 0xf3f5f8: case 0xffffff: color = 0x18212c; break;
            default: break;
        }
    }
    return lv_color_hex(color);
}

void flush(lv_disp_drv_t* driver, const lv_area_t* area, lv_color_t* colors) {
    const bool present = lv_disp_flush_is_last(driver);
    if (!platform_display_flush_pixels(colors, area->x1, area->y1, lv_area_get_width(area),
                                       lv_area_get_height(area), present)) LV_LOG_ERROR("Display strip upload failed");
    else if (present) ++fps_frame_count;
    lv_disp_flush_ready(driver);
}

void read_pointer(lv_indev_drv_t*, lv_indev_data_t* data) {
    *data = pointer_data;
}

lv_obj_t* scroll_target_at(lv_obj_t* root, const lv_point_t& point, int direction) {
    if (lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) return nullptr;
    lv_area_t bounds;
    lv_obj_get_coords(root, &bounds);
    if (point.x < bounds.x1 || point.x > bounds.x2 || point.y < bounds.y1 || point.y > bounds.y2) return nullptr;
    for (uint32_t child_index = lv_obj_get_child_cnt(root); child_index > 0; --child_index) {
        lv_obj_t* target = scroll_target_at(lv_obj_get_child(root, child_index - 1), point, direction);
        if (target) return target;
    }
    if (!lv_obj_check_type(root, &lv_label_class) && lv_obj_has_flag(root, LV_OBJ_FLAG_SCROLLABLE) &&
        (lv_obj_get_scroll_dir(root) & LV_DIR_VER) &&
        ((direction > 0 && lv_obj_get_scroll_top(root) > 0) ||
         (direction < 0 && lv_obj_get_scroll_bottom(root) > 0))) return root;
    return nullptr;
}

const lv_font_t* ui_font() {
    return platform_ui_font(&lv_font_montserrat_16);
}

const lv_font_t* font_for_text(const char* text, int size) {
    // LV_SYMBOL_* values are UTF-8 private-use glyphs (EF 80/81 xx). They are
    // supplied by LVGL's Montserrat font, not the Windows CJK FreeType face.
    // Routing them to ui_font() makes detail/player rendering spin forever
    // in lv_font_get_glyph_bitmap when the font has no matching glyph.
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(text);
    if (bytes[0] == 0xef && (bytes[1] == 0x80 || bytes[1] == 0x81)) {
        if (size <= 12) return &lv_font_montserrat_12;
        if (size <= 14) return &lv_font_montserrat_14;
        if (size >= 24) return &lv_font_montserrat_24;
        if (size >= 20) return &lv_font_montserrat_20;
        return &lv_font_montserrat_16;
    }
    // Generated board fonts carry CJK text. Use the compact built-in ASCII
    // faces for narrow navigation controls so 800x480 never truncates labels.
    for (const unsigned char* cursor = bytes; *cursor; ++cursor) {
        if (*cursor & 0x80) {
#ifdef WILIWILI_BOARD
            return platform_ui_font(font_for_text("", size));
#else
            return ui_font();
#endif
        }
    }
    if (size <= 12) return &lv_font_montserrat_12;
    if (size <= 14) return &lv_font_montserrat_14;
    if (size >= 24) return &lv_font_montserrat_24;
    if (size >= 20) return &lv_font_montserrat_20;
    return &lv_font_montserrat_16;
}

void lock_static_canvas(lv_obj_t* object) {
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(object, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(object, LV_DIR_NONE);
    // Some LVGL button/object classes retain their scrollbar part styling
    // after the scrollable flag is cleared. Hide that part explicitly so
    // fixed cards cannot show phantom horizontal/vertical tracks.
    lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(object, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_height(object, 0, LV_PART_SCROLLBAR);
}

void set_label(lv_obj_t* parent, const char* text, int size, lv_color_t color,
               lv_align_t align, int x, int y) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font_for_text(text, size), 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    lv_obj_set_style_text_line_space(label, 4, 0);
    lv_obj_align(label, align, x, y);
}

bool set_label_text_if_changed(lv_obj_t* label, const char* text) {
    if (!label || std::strcmp(lv_label_get_text(label), text) == 0) return false;
    lv_label_set_text(label, text);
    return true;
}

void style_dark_textarea(lv_obj_t* input) {
    lv_obj_set_style_bg_color(input, ui_color(0x2b3444), 0);
    lv_obj_set_style_text_color(input, ui_color(0xf3f5f8), 0);
    lv_obj_set_style_text_color(input, ui_color(0xaeb8c6), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_border_width(input, 0, 0);
    lv_obj_set_style_radius(input, 5, 0);
    lv_obj_set_style_text_font(input, ui_font(), 0);
}

void set_symbol(lv_obj_t* parent, const char* symbol, lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, symbol);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

std::string decode_json_string(const std::string& encoded) {
    const auto append_utf8 = [](std::string& output, uint32_t codepoint) {
        if (codepoint < 0x80) output += static_cast<char>(codepoint);
        else if (codepoint < 0x800) {
            output += static_cast<char>(0xc0 | (codepoint >> 6));
            output += static_cast<char>(0x80 | (codepoint & 0x3f));
        } else {
            output += static_cast<char>(0xe0 | (codepoint >> 12));
            output += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
            output += static_cast<char>(0x80 | (codepoint & 0x3f));
        }
    };
    const auto hex_value = [](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    };
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
            case 'u': {
                if (i + 4 >= encoded.size()) {
                    decoded += 'u';
                    break;
                }
                uint32_t codepoint = 0;
                bool valid = true;
                for (size_t digit = 0; digit < 4; ++digit) {
                    const int value = hex_value(encoded[i + 1 + digit]);
                    if (value < 0) {
                        valid = false;
                        break;
                    }
                    codepoint = (codepoint << 4) | static_cast<uint32_t>(value);
                }
                if (valid) {
                    append_utf8(decoded, codepoint);
                    i += 4;
                } else {
                    decoded += 'u';
                }
                break;
            }
            default: decoded += next; break;
        }
    }
    // The deployed CJK font has no reliable glyph fallback for non-BMP emoji.
    // Letting a four-byte emoji reach LVGL's FreeType path can stall rendering
    // indefinitely, so remove those decorative codepoints at the data boundary.
    std::string display_safe;
    for (size_t index = 0; index < decoded.size();) {
        const unsigned char lead = static_cast<unsigned char>(decoded[index]);
        if (lead >= 0xf0 && lead <= 0xf4 && index + 3 < decoded.size()) {
            index += 4;
            continue;
        }
        // JSON surrogate halves decoded independently are equally unusable.
        if (lead == 0xed && index + 2 < decoded.size() &&
            (static_cast<unsigned char>(decoded[index + 1]) & 0xe0) == 0xa0) {
            index += 3;
            continue;
        }
        display_safe += decoded[index++];
    }
    return display_safe;
}

std::string strip_html_tags(const std::string& text) {
    std::string clean;
    bool in_tag = false;
    for (char character : text) {
        if (character == '<') in_tag = true;
        else if (character == '>') in_tag = false;
        else if (!in_tag) clean += character;
    }
    return clean;
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

bool json_string_in_range(const std::string& object, const char* key, size_t begin, size_t end,
                          std::string& value) {
    if (begin >= object.size()) return false;
    end = std::min(end, object.size());
    const std::string slice = object.substr(begin, end - begin);
    return json_string_at(slice, key, value);
}

bool parse_play_source(const std::string& body, std::string& url, std::string& format,
                       int preferred_codec = 7, std::string* audio_url = nullptr) {
    const size_t durl = body.find("\"durl\":[");
    if (durl != std::string::npos) {
        const size_t object_start = body.find('{', durl);
        const size_t object_end = body.find('}', object_start);
        if (object_start != std::string::npos && object_end != std::string::npos &&
            json_string_in_range(body, "url", object_start, object_end + 1, url)) {
            format = "durl";
            return true;
        }
    }
    const size_t dash = body.find("\"dash\":{");
    if (dash != std::string::npos) {
        const size_t video = body.find("\"video\":[", dash);
        if (video != std::string::npos) {
            size_t cursor = video;
            std::string fallback;
            for (int count = 0; count < 16; ++count) {
                const size_t object_start = body.find('{', cursor);
                if (object_start == std::string::npos) break;
                const size_t object_end = body.find('}', object_start);
                if (object_end == std::string::npos) break;
                const std::string object = body.substr(object_start, object_end - object_start + 1);
                std::string candidate;
                if (json_string_at(object, "baseUrl", candidate) || json_string_at(object, "base_url", candidate) ||
                    json_string_at(object, "backupUrl", candidate) || json_string_at(object, "backup_url", candidate)) {
                    if (fallback.empty()) fallback = candidate;
                    std::string codecid;
                    if (json_number_at(object, "codecid", codecid) && std::stoi(codecid) == preferred_codec) {
                        fallback = candidate;
                        break;
                    }
                }
                cursor = object_end + 1;
            }
            if (!fallback.empty()) {
                url = fallback;
                format = "dash";
                const size_t audio = body.find("\"audio\":[", dash);
                const size_t audio_start = audio == std::string::npos ? std::string::npos : body.find('{', audio);
                const size_t audio_end = audio_start == std::string::npos ? std::string::npos : body.find('}', audio_start);
                if (audio_url && audio_start != std::string::npos && audio_end != std::string::npos) {
                    json_string_in_range(body, "baseUrl", audio_start, audio_end + 1, *audio_url) ||
                        json_string_in_range(body, "base_url", audio_start, audio_end + 1, *audio_url);
                }
                return true;
            }
        }
    }
    return false;
}

std::vector<VideoPart> parse_video_parts(const std::string& body) {
    std::vector<VideoPart> parts;
    const size_t pages = body.find("\"pages\":[");
    if (pages == std::string::npos) return parts;
    size_t cursor = pages;
    while (parts.size() < 32) {
        const size_t start = body.find("{\"cid\":", cursor);
        if (start == std::string::npos) break;
        int depth = 0;
        size_t end = start;
        for (; end < body.size(); ++end) {
            if (body[end] == '{') ++depth;
            else if (body[end] == '}' && --depth == 0) {
                ++end;
                break;
            }
        }
        if (depth != 0) break;
        const std::string page = body.substr(start, end - start);
        VideoPart part;
        json_number_at(page, "cid", part.cid);
        json_number_at(page, "duration", part.duration);
        json_string_at(page, "part", part.title);
        if (!part.cid.empty()) parts.push_back(std::move(part));
        cursor = end;
    }
    return parts;
}

size_t json_object_end(const std::string& source, size_t start) {
    int depth = 0;
    bool quoted = false;
    bool escaped = false;
    for (size_t index = start; index < source.size(); ++index) {
        const char character = source[index];
        if (quoted) {
            if (escaped) escaped = false;
            else if (character == '\\') escaped = true;
            else if (character == '"') quoted = false;
            continue;
        }
        if (character == '"') quoted = true;
        else if (character == '{') ++depth;
        else if (character == '}' && --depth == 0) return index + 1;
    }
    return std::string::npos;
}

void truncate_utf8_text(std::string& text, size_t max_bytes) {
    if (text.size() <= max_bytes) return;
    size_t end = max_bytes;
    while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0U) == 0x80U) --end;
    text.resize(end);
    text.shrink_to_fit();
}

std::vector<VideoComment> parse_video_comments(const std::string& body) {
    std::vector<VideoComment> comments;
    const size_t replies = body.find("\"replies\":[");
    if (replies == std::string::npos) return comments;
    size_t cursor = replies;
    while (comments.size() < 8) {
        const size_t start = body.find('{', cursor);
        if (start == std::string::npos) break;
        const size_t end = json_object_end(body, start);
        if (end == std::string::npos) break;
        const std::string reply = body.substr(start, end - start);
        VideoComment comment;
        json_string_at(reply, "uname", comment.author);
        json_string_at(reply, "message", comment.message);
        truncate_utf8_text(comment.author, kMaxCommentAuthorBytes);
        truncate_utf8_text(comment.message, kMaxCommentMessageBytes);
        if (!is_valid_utf8(comment.author) || !is_valid_utf8(comment.message)) {
            cursor = end;
            continue;
        }
        if (!comment.author.empty() && !comment.message.empty()) comments.push_back(std::move(comment));
        cursor = end;
    }
    return comments;
}

bool merge_comment_page(CommentPageResult& result) {
    if (result.epoch != navigation_epoch.load() || result.video_index >= displayed_videos.size() ||
        result.video_index != active_video_index || result.page != comment_next_page ||
        displayed_videos[result.video_index].bvid != result.bvid || !result.error.empty()) return false;
    Video& video = displayed_videos[result.video_index];
    const size_t previous_count = video.comments.size();
    for (VideoComment& comment : result.comments) {
        if (video.comments.size() >= kMaxVideoComments) break;
        const bool duplicate = std::any_of(video.comments.begin(), video.comments.end(), [&comment](const VideoComment& item) {
            return item.author == comment.author && item.message == comment.message;
        });
        if (!duplicate) video.comments.push_back(std::move(comment));
    }
    video.comments_exhausted = video.comments.size() >= kMaxVideoComments || video.comments.size() == previous_count;
    ++comment_next_page;
    return true;
}

std::vector<RelatedVideo> parse_related_videos(const std::string& body) {
    std::vector<RelatedVideo> related;
    const size_t data = body.find("\"data\":[");
    if (data == std::string::npos) return related;
    size_t cursor = body.find('{', data);
    while (related.size() < 4 && cursor != std::string::npos) {
        const size_t start = cursor;
        const size_t end = json_object_end(body, start);
        if (end == std::string::npos) break;
        const std::string item = body.substr(start, end - start);
        RelatedVideo video;
        json_string_at(item, "bvid", video.bvid);
        json_string_at(item, "title", video.title);
        const size_t owner = item.find("\"owner\":{");
        if (owner != std::string::npos) json_string_at(item.substr(owner), "name", video.owner);
        const size_t stat = item.find("\"stat\":{");
        if (stat != std::string::npos) json_number_at(item.substr(stat), "view", video.view);
        if (!video.bvid.empty() && !video.title.empty()) related.push_back(std::move(video));
        cursor = body.find('{', end);
    }
    return related;
}

std::vector<HotTerm> parse_hot_terms(const std::string& body) {
    std::vector<HotTerm> terms;
    const size_t list = body.find("\"list\":[");
    if (list == std::string::npos) return terms;
    size_t cursor = body.find('{', list);
    while (terms.size() < 8 && cursor != std::string::npos) {
        const size_t end = json_object_end(body, cursor);
        if (end == std::string::npos) break;
        const std::string item = body.substr(cursor, end - cursor);
        HotTerm term;
        json_string_at(item, "keyword", term.keyword);
        json_number_at(item, "heat_score", term.heat);
        if (!term.keyword.empty()) terms.push_back(std::move(term));
        cursor = body.find('{', end);
    }
    return terms;
}

std::vector<MediaSearchItem> parse_media_search(const std::string& body) {
    std::vector<MediaSearchItem> results;
    const size_t list = body.find("\"result\":[");
    if (list == std::string::npos) return results;
    size_t cursor = body.find('{', list);
    while (results.size() < 8 && cursor != std::string::npos) {
        const size_t end = json_object_end(body, cursor);
        if (end == std::string::npos) break;
        const std::string item = body.substr(cursor, end - cursor);
        MediaSearchItem result;
        json_number_at(item, "season_id", result.id);
        if (result.id.empty()) json_number_at(item, "media_id", result.id);
        json_string_at(item, "title", result.title);
        result.title = strip_html_tags(result.title);
        json_string_at(item, "index_show", result.subtitle);
        if (result.subtitle.empty()) json_string_at(item, "org_title", result.subtitle);
        if (!result.title.empty()) results.push_back(std::move(result));
        cursor = body.find('{', end);
    }
    if (!results.empty()) return results;
    // Search responses occasionally add wrapper objects around each media
    // entry. Fall back to a bounded field window instead of depending on
    // object nesting or field order.
    size_t fallback_cursor = list;
    while (results.size() < 8) {
        const size_t title_marker = body.find("\"title\":\"", fallback_cursor);
        if (title_marker == std::string::npos) break;
        const size_t window_end = std::min(body.size(), title_marker + 900);
        MediaSearchItem result;
        json_string_at(body.substr(title_marker, window_end - title_marker), "title", result.title);
        result.title = strip_html_tags(result.title);
        const std::string window = body.substr(title_marker, window_end - title_marker);
        json_number_at(window, "season_id", result.id);
        if (result.id.empty()) json_number_at(window, "media_id", result.id);
        json_string_at(window, "index_show", result.subtitle);
        if (!result.title.empty()) results.push_back(std::move(result));
        fallback_cursor = title_marker + 8;
    }
    return results;
}

SeasonDetail parse_season_detail(const std::string& body, const std::string& season_id) {
    SeasonDetail season;
    season.season_id = season_id;
    const size_t result = body.find("\"result\":{");
    const std::string root = result == std::string::npos ? body : body.substr(result);
    json_string_at(root, "title", season.title);
    json_string_at(root, "subtitle", season.subtitle);
    json_string_at(root, "evaluate", season.description);
    if (season.description.empty()) json_string_at(root, "share_sub_title", season.description);

    const size_t episodes = root.find("\"episodes\":[");
    if (episodes == std::string::npos) return season;
    size_t cursor = root.find('{', episodes);
    while (season.episodes.size() < 32 && cursor != std::string::npos) {
        const size_t end = json_object_end(root, cursor);
        if (end == std::string::npos) break;
        const std::string item = root.substr(cursor, end - cursor);
        SeasonEpisode episode;
        json_string_at(item, "bvid", episode.bvid);
        json_number_at(item, "cid", episode.cid);
        json_string_at(item, "long_title", episode.title);
        if (episode.title.empty()) json_string_at(item, "title", episode.title);
        json_string_at(item, "badge", episode.badge);
        if (!episode.bvid.empty()) season.episodes.push_back(std::move(episode));
        cursor = root.find('{', end);
    }
    return season;
}

std::vector<Video> parse_feed(const std::string& body, bool popular) {
    std::vector<Video> videos;
    size_t cursor = 0;
    while (videos.size() < 8) {
        const size_t marker = body.find("\"bvid\":\"", cursor);
        if (marker == std::string::npos) break;
        const size_t start = body.rfind('{', marker);
        const size_t end = body.find(popular ? "\"dimension\":" : "\"track_id\":", marker);
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

std::vector<Video> parse_search(const std::string& body) {
    std::vector<Video> videos;
    size_t cursor = 0;
    while (videos.size() < 8) {
        const size_t marker = body.find("\"bvid\":\"", cursor);
        if (marker == std::string::npos) break;
        const size_t start = body.rfind('{', marker);
        const size_t end = body.find("\"pubdate\":", marker);
        if (start == std::string::npos || end == std::string::npos) break;
        const std::string object = body.substr(start, end - start);
        Video video;
        json_string_at(object, "bvid", video.bvid);
        json_string_at(object, "title", video.title);
        json_string_at(object, "pic", video.pic);
        json_string_at(object, "author", video.owner);
        if (!json_string_at(object, "play", video.view)) json_number_at(object, "play", video.view);
        video.title = strip_html_tags(video.title);
        if (!video.bvid.empty() && !video.title.empty()) videos.push_back(std::move(video));
        cursor = end;
    }
    return videos;
}

std::vector<Video> parse_video_list_loose(const std::string& body) {
    std::vector<Video> videos;
    size_t cursor = 0;
    while (videos.size() < 6) {
        const size_t marker = body.find("\"bvid\":\"", cursor);
        if (marker == std::string::npos) break;
        const size_t start = marker + 8;
        const size_t end = body.find('"', start);
        if (end == std::string::npos) break;
        const size_t object_start = body.rfind('{', marker);
        const size_t object_end = body.find('}', end);
        if (object_start == std::string::npos || object_end == std::string::npos) break;
        const std::string object = body.substr(object_start, object_end - object_start + 1);
        Video video;
        video.bvid = body.substr(start, end - start);
        json_string_at(object, "title", video.title);
        json_string_at(object, "pic", video.pic);
        if (video.pic.empty()) json_string_at(object, "cover", video.pic);
        json_string_at(object, "author", video.owner);
        if (video.owner.empty()) json_string_at(object, "upper", video.owner);
        if (!video.title.empty()) videos.push_back(std::move(video));
        cursor = end + 1;
    }
    return videos;
}

std::string url_encode(const std::string& value) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
            encoded += static_cast<char>(c);
        } else {
            encoded += '%';
            encoded += hex[c >> 4];
            encoded += hex[c & 0x0f];
        }
    }
    return encoded;
}

bool fetch_raw_bilibili_path(const std::string& path, std::string& response, std::string& error) {
    std::vector<uint8_t> bytes;
    if (!platform_https_get(std::string("https://api.bilibili.com") + path, bilibili_headers(), bytes, error)) {
        return false;
    }
    response.assign(bytes.begin(), bytes.end());
    return true;
}

void report_playback_history(const Video& video, uint32_t progress_seconds = 0) {
    if (!setting_history_report) return;
    if (bilibili_cookie.empty() || video.aid.empty() || video.cid.empty()) return;
    const std::string mid = cookie_field("DedeUserID");
    const std::string csrf = cookie_field("bili_jct");
    if (mid.empty() || csrf.empty()) return;
    unsigned long duration = 0;
    try { duration = std::stoul(video.duration); } catch (...) { duration = 0; }
    const std::string form = "mid=" + url_encode(mid) +
                             "&csrf=" + url_encode(csrf) +
                             "&aid=" + url_encode(video.aid) +
                             "&cid=" + url_encode(video.cid) +
                             "&progress=" + std::to_string(progress_seconds) + "&duration=" + std::to_string(duration) +
                             "&type=3&dt=9";
    std::thread([form] {
        std::vector<uint8_t> body;
        std::string error;
        if (!platform_https_post("https://api.bilibili.com/x/v2/history/report",
                                 bilibili_headers(), form, body, error)) return;
    }).detach();
}

void like_active_video(lv_event_t* event) {
    if (active_video_index >= displayed_videos.size()) return;
    const Video video = displayed_videos[active_video_index];
    const std::string csrf = cookie_field("bili_jct");
    if (video.aid.empty() || csrf.empty()) {
        if (status_label) lv_label_set_text(status_label, "Like requires a signed-in Cookie session");
        return;
    }
    std::thread([video, csrf] {
        std::vector<uint8_t> body;
        std::string error;
        const std::string form = "aid=" + url_encode(video.aid) + "&like=1&csrf=" + url_encode(csrf);
        const bool ok = platform_https_post("https://api.bilibili.com/x/web-interface/archive/like",
                                            bilibili_headers(), form, body, error);
        std::string code;
        const std::string response(body.begin(), body.end());
        const bool accepted = ok && json_number_at(response, "code", code) && code == "0";
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_action_result = accepted ? "Like accepted" : "Like failed";
        }
        action_result_complete = true;
    }).detach();
    lv_obj_t* button = static_cast<lv_obj_t*>(lv_event_get_target(event));
    lv_obj_t* label = lv_obj_get_child(button, 0);
    if (label) lv_label_set_text(label, "Like sent");
    if (status_label) lv_label_set_text(status_label, "Like request sent");
}

void favorite_active_video(lv_event_t*) {
    if (active_video_index >= displayed_videos.size()) return;
    const Video video = displayed_videos[active_video_index];
    const std::string csrf = cookie_field("bili_jct");
    const std::string mid = cookie_field("DedeUserID");
    if (video.aid.empty() || csrf.empty() || mid.empty()) {
        if (status_label) lv_label_set_text(status_label, "Favorite requires a signed-in Cookie session");
        return;
    }
    std::thread([video, csrf, mid] {
        std::string folders;
        std::string error;
        std::string media_id;
        if (fetch_raw_bilibili_path("/x/v3/fav/folder/created/list?up_mid=" + url_encode(mid) + "&jsonp=jsonp",
                                    folders, error)) {
            json_number_at(folders, "media_id", media_id);
            if (media_id.empty()) json_number_at(folders, "id", media_id);
        }
        std::vector<uint8_t> body;
        std::string code;
        const bool ok = !media_id.empty() && platform_https_post(
            "https://api.bilibili.com/x/v3/fav/resource/deal", bilibili_headers(),
            "rid=" + url_encode(video.aid) + "&type=2&add_media_ids=" + url_encode(media_id) +
            "&del_media_ids=&csrf=" + url_encode(csrf), body, error) &&
            json_number_at(std::string(body.begin(), body.end()), "code", code) && code == "0";
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_action_result = ok ? "Favorite accepted" : "Favorite failed";
        }
        if (ok) refresh_favorites_after_action = true;
        action_result_complete = true;
    }).detach();
    if (status_label) lv_label_set_text(status_label, "Favorite request sent");
}

void remove_favorite_video(lv_event_t* event) {
    lv_event_stop_bubbling(event);
    const size_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    if (index >= displayed_favorite_videos.size()) return;
    const Video video = displayed_favorite_videos[index];
    const std::string csrf = cookie_field("bili_jct");
    const std::string mid = cookie_field("DedeUserID");
    if (video.aid.empty() || csrf.empty() || mid.empty()) {
        if (status_label) lv_label_set_text(status_label, "Remove requires a signed-in Cookie session");
        return;
    }
    std::thread([video, csrf, mid] {
        std::string folders;
        std::string error;
        std::string media_id;
        if (fetch_raw_bilibili_path("/x/v3/fav/folder/created/list?up_mid=" + url_encode(mid) + "&jsonp=jsonp",
                                    folders, error)) {
            json_number_at(folders, "media_id", media_id);
            if (media_id.empty()) json_number_at(folders, "id", media_id);
        }
        std::vector<uint8_t> body;
        std::string code;
        const bool ok = !media_id.empty() && platform_https_post(
            "https://api.bilibili.com/x/v3/fav/resource/deal", bilibili_headers(),
            "rid=" + url_encode(video.aid) + "&type=2&add_media_ids=&del_media_ids=" + url_encode(media_id) +
            "&csrf=" + url_encode(csrf), body, error) &&
            json_number_at(std::string(body.begin(), body.end()), "code", code) && code == "0";
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_action_result = ok ? "Removed from favorites" : "Favorite removal failed";
        }
        if (ok) refresh_favorites_after_action = true;
        action_result_complete = true;
    }).detach();
    if (status_label) lv_label_set_text(status_label, "Removing from favorites...");
}

void submit_video_comment(lv_event_t*) {
    if (!detail_comment_input || active_video_index >= displayed_videos.size()) return;
    const std::string message = lv_textarea_get_text(detail_comment_input);
    const Video video = displayed_videos[active_video_index];
    const std::string csrf = cookie_field("bili_jct");
    if (message.empty() || video.aid.empty() || csrf.empty()) {
        if (status_label) lv_label_set_text(status_label, "Comment requires text and a signed-in Cookie session");
        return;
    }
    std::thread([video, csrf, message] {
        std::vector<uint8_t> body;
        std::string error;
        const std::string form = "oid=" + url_encode(video.aid) + "&type=1&message=" + url_encode(message) +
                                 "&plat=1&csrf=" + url_encode(csrf);
        const bool ok = platform_https_post("https://api.bilibili.com/x/v2/reply/add",
                                            bilibili_headers(), form, body, error);
        std::string code;
        const std::string response(body.begin(), body.end());
        const bool accepted = ok && json_number_at(response, "code", code) && code == "0";
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_action_result = accepted ? "Comment accepted" : "Comment failed";
        }
        if (accepted) refresh_detail_after_action = true;
        action_result_complete = true;
    }).detach();
    lv_textarea_set_text(detail_comment_input, "");
    if (status_label) lv_label_set_text(status_label, "Comment submitted");
}

std::vector<DanmakuItem> parse_danmaku_xml(const std::string& body) {
    std::vector<DanmakuItem> result;
    size_t cursor = 0;
    while (result.size() < 12) {
        const size_t tag = body.find("<d ", cursor);
        if (tag == std::string::npos) break;
        const size_t start = body.find(">", tag);
        if (start == std::string::npos) break;
        const size_t end = body.find("</d>", start + 1);
        if (end == std::string::npos) break;
        std::string text = xml_unescape(body.substr(start + 1, end - start - 1));
        text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
        text.erase(std::remove(text.begin(), text.end(), '\n'), text.end());
        const size_t p = body.find("p=\"", tag);
        const size_t comma = p == std::string::npos ? std::string::npos : body.find(',', p + 3);
        const std::string seconds = comma == std::string::npos ? std::string{} : body.substr(p + 3, comma - p - 3);
        if (!text.empty() && text.size() <= 160 && !seconds.empty()) {
            DanmakuItem item;
            try { item.time_ms = static_cast<uint32_t>(std::stof(seconds) * 1000.0f); }
            catch (...) { item.time_ms = 0; }
            item.text = std::move(text);
            result.push_back(std::move(item));
        }
        cursor = end + 4;
    }
    return result;
}

void request_danmaku(size_t video_index) {
    if (!setting_danmaku_on) return;
    if (video_index >= displayed_videos.size() || displayed_videos[video_index].cid.empty() ||
        danmaku_fetch_in_flight.exchange(true)) return;
    const std::string cid = displayed_videos[video_index].cid;
    std::thread([cid, video_index] {
        std::vector<uint8_t> bytes;
        std::vector<DanmakuItem> result;
        std::string error;
        if (platform_https_get("https://comment.bilibili.com/" + cid + ".xml",
                               bilibili_headers(), bytes, error) && bytes.size() <= 2 * 1024 * 1024) {
            result = parse_danmaku_xml(std::string(bytes.begin(), bytes.end()));
        }
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_danmaku = std::move(result);
            pending_danmaku_index = video_index;
        }
        danmaku_fetch_in_flight = false;
        danmaku_fetch_complete = true;
    }).detach();
}

void refresh_danmaku_overlay() {
    if (!danmaku_label) return;
    if (!setting_danmaku_on || !player_playing || active_video_index >= displayed_videos.size()) {
        for (lv_obj_t* track : danmaku_tracks) set_label_text_if_changed(track, "");
        return;
    }
    const uint32_t wall_ms = player_playing ? platform_ticks() - player_started_tick : 0;
    const uint32_t elapsed = player_elapsed_offset_ms + wall_ms * static_cast<uint32_t>(setting_playback_speed) / 100U;
    std::array<const DanmakuItem*, 3> active{};
    for (const DanmakuItem& item : displayed_videos[active_video_index].danmaku) {
        if (item.time_ms > elapsed || elapsed - item.time_ms > 5000) continue;
        const size_t track = static_cast<size_t>((item.time_ms / 1000U) % active.size());
        if (!active[track] || item.time_ms > active[track]->time_ms) active[track] = &item;
    }
    const uint32_t duration_ms = static_cast<uint32_t>(500000U / std::max(50, setting_danmaku_speed));
    for (size_t index = 0; index < danmaku_tracks.size(); ++index) {
        lv_obj_t* track = danmaku_tracks[index];
        if (!track) continue;
        const DanmakuItem* item = active[index];
        if (!item) {
            set_label_text_if_changed(track, "");
            continue;
        }
        set_label_text_if_changed(track, item->text.c_str());
        const uint32_t age = elapsed - item->time_ms;
        const int travel = lv_obj_get_width(track) + 720;
        lv_obj_set_x(track, 720 - static_cast<int>(travel * std::min(age, duration_ms) / duration_ms));
    }
}

std::string wbi_key_from_url(const std::string& url) {
    const size_t slash = url.find_last_of('/');
    const size_t dot = url.find_last_of('.');
    return slash != std::string::npos && dot != std::string::npos && slash < dot ?
        url.substr(slash + 1, dot - slash - 1) : std::string{};
}

bool update_wbi_key(std::string& error) {
    const std::time_t now = std::time(nullptr);
    if (!wbi_mixin_key.empty() && now - wbi_key_update_time < 3600) return true;
    std::string response;
    if (!fetch_raw_bilibili_path("/x/web-interface/nav", response, error)) return false;
    std::string img_url;
    std::string sub_url;
    if (!json_string_at(response, "img_url", img_url) || !json_string_at(response, "sub_url", sub_url)) {
        error = "Bilibili WBI keys were not present in nav response";
        return false;
    }
    const std::string raw_key = wbi_key_from_url(img_url) + wbi_key_from_url(sub_url);
    if (raw_key.size() < kWbiMixinTable.size()) {
        error = "Bilibili WBI keys were malformed";
        return false;
    }
    wbi_mixin_key.clear();
    wbi_mixin_key.reserve(32);
    for (size_t index = 0; index < 32; ++index) wbi_mixin_key += raw_key[kWbiMixinTable[index]];
    wbi_key_update_time = now;
    return true;
}

bool sign_wbi_path(const std::string& path, std::string& signed_path, std::string& error) {
    if (!update_wbi_key(error)) return false;
    const size_t question = path.find('?');
    const std::string base = question == std::string::npos ? path : path.substr(0, question);
    const std::string query = question == std::string::npos ? std::string{} : path.substr(question + 1);
    std::map<std::string, std::string> parameters;
    size_t cursor = 0;
    while (cursor <= query.size()) {
        const size_t separator = query.find('&', cursor);
        const std::string pair = query.substr(cursor, separator == std::string::npos ? std::string::npos : separator - cursor);
        const size_t equal = pair.find('=');
        if (equal != std::string::npos) parameters[pair.substr(0, equal)] = pair.substr(equal + 1);
        if (separator == std::string::npos) break;
        cursor = separator + 1;
    }
    parameters["wts"] = std::to_string(std::time(nullptr));
    std::string canonical;
    for (const auto& parameter : parameters) {
        const auto& key = parameter.first;
        const auto& source_value = parameter.second;
        std::string value = source_value;
        value.erase(std::remove_if(value.begin(), value.end(), [](char c) {
            return c == '!' || c == '\'' || c == '(' || c == ')' || c == '*';
        }), value.end());
        const std::string encoded = url_encode(value);
        if (!canonical.empty()) canonical += '&';
        canonical += key + "=" + encoded;
    }
    std::string digest;
    if (!platform_md5_hex(canonical + wbi_mixin_key, digest, error)) return false;
    signed_path = base + "?" + canonical + "&w_rid=" + digest;
    return true;
}

bool fetch_feed(const std::string& path, std::string& response, std::string& error) {
    // Most feed, search, and public archive endpoints are usable without WBI
    // signing. Prefer that path: it avoids making a working recommendation
    // request depend on a separate /nav key fetch and local MD5 implementation.
    // Endpoints that actually require a signature return a non-zero API code,
    // then fall through to the signed retry below.
    std::string direct_response;
    std::string direct_error;
    if (fetch_raw_bilibili_path(path, direct_response, direct_error)) {
        std::string direct_code;
        if (json_number_at(direct_response, "code", direct_code) && direct_code == "0") {
            response = std::move(direct_response);
            error.clear();
            return true;
        }
    }

    std::string signed_path;
    if (!sign_wbi_path(path, signed_path, error)) {
        if (!direct_error.empty()) error = direct_error;
        return false;
    }
    return fetch_raw_bilibili_path(signed_path, response, error);
}

void limit_dynamic_label(std::string& text, size_t max_bytes) {
    size_t cursor = 0;
    while (cursor < text.size() && cursor < max_bytes) {
        const unsigned char lead = static_cast<unsigned char>(text[cursor]);
        const size_t width = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
        if (cursor + width > text.size() || cursor + width > max_bytes) break;
        cursor += width;
    }
    text.resize(cursor);
}

wiliwili::string_view dynamic_member(wiliwili::string_view object, wiliwili::string_view key) {
    if (object.empty() || object.front() != '{') return {};
    size_t cursor = 1;
    const auto skip_space = [&object, &cursor] {
        while (cursor < object.size() && (object[cursor] == ' ' || object[cursor] == '\n' ||
               object[cursor] == '\r' || object[cursor] == '\t')) ++cursor;
    };
    while (cursor < object.size()) {
        skip_space();
        if (cursor >= object.size() || object[cursor] != '"') return {};
        const size_t name_start = ++cursor;
        while (cursor < object.size() && object[cursor] != '"') {
            if (object[cursor] == 0x5c && cursor + 1 < object.size()) ++cursor;
            ++cursor;
        }
        if (cursor >= object.size()) return {};
        const wiliwili::string_view name = object.substr(name_start, cursor++ - name_start);
        skip_space();
        if (cursor >= object.size() || object[cursor++] != ':') return {};
        skip_space();
        const size_t value_start = cursor;
        if (cursor >= object.size()) return {};
        if (object[cursor] == '"') {
            ++cursor;
            while (cursor < object.size()) {
                if (object[cursor] == 0x5c && cursor + 1 < object.size()) { cursor += 2; continue; }
                if (object[cursor++] == '"') break;
            }
            if (cursor > object.size() || object[cursor - 1] != '"') return {};
        } else if (object[cursor] == '{' || object[cursor] == '[') {
            int depth = 0;
            bool in_string = false;
            bool escaped = false;
            for (; cursor < object.size(); ++cursor) {
                const char character = object[cursor];
                if (in_string) {
                    if (escaped) escaped = false;
                    else if (character == 0x5c) escaped = true;
                    else if (character == '"') in_string = false;
                } else if (character == '"') in_string = true;
                else if (character == '{' || character == '[') ++depth;
                else if ((character == '}' || character == ']') && --depth == 0) { ++cursor; break; }
            }
            if (depth != 0) return {};
        } else {
            while (cursor < object.size() && object[cursor] != ',' && object[cursor] != '}') ++cursor;
        }
        if (name == key) return object.substr(value_start, cursor - value_start);
        skip_space();
        if (cursor >= object.size() || object[cursor] != ',') return {};
        ++cursor;
    }
    return {};
}

wiliwili::string_view dynamic_module(wiliwili::string_view modules, wiliwili::string_view key) {
    if (modules.empty()) return {};
    if (modules.front() == '{') return dynamic_member(modules, key);
    if (modules.front() != '[') return {};
    size_t cursor = 1;
    while (cursor < modules.size()) {
        while (cursor < modules.size() && (modules[cursor] == ',' || modules[cursor] == ' ' ||
               modules[cursor] == '\n' || modules[cursor] == '\r' || modules[cursor] == '\t')) ++cursor;
        if (cursor >= modules.size() || modules[cursor] != '{') break;
        const size_t start = cursor;
        int depth = 0;
        bool in_string = false;
        bool escaped = false;
        for (; cursor < modules.size(); ++cursor) {
            const char character = modules[cursor];
            if (in_string) {
                if (escaped) escaped = false;
                else if (character == 0x5c) escaped = true;
                else if (character == '"') in_string = false;
            } else if (character == '"') in_string = true;
            else if (character == '{') ++depth;
            else if (character == '}' && --depth == 0) { ++cursor; break; }
        }
        if (depth != 0) break;
        const wiliwili::string_view value = dynamic_member(modules.substr(start, cursor - start), key);
        if (!value.empty()) return value;
    }
    return {};
}

std::string dynamic_text(wiliwili::string_view object, wiliwili::string_view key, size_t max_bytes) {
    const wiliwili::string_view value = dynamic_member(object, key);
    if (value.size() < 2 || value.front() != '"' || value.back() != '"') return {};
    std::string text = decode_json_string(std::string(value.substr(1, std::min(value.size() - 2, size_t{1024}))));
    limit_dynamic_label(text, max_bytes);
    return text;
}

void dynamic_row_line(lv_obj_t* row, const std::string& text, int size, lv_color_t color,
                      int width, int top) {
    lv_obj_t* label = lv_label_create(row);
    lv_obj_set_style_text_font(label, font_for_text(text.c_str(), size), 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(label, width, 20);
    lv_obj_set_pos(label, 12, top);
    lv_label_set_text(label, text.c_str());
}

std::vector<DynamicItem> parse_dynamic_items(const std::string& body) {
    std::vector<DynamicItem> result;
    const size_t items = body.find("\"items\"");
    if (items == std::string::npos) return result;
    size_t cursor = body.find('[', items + 7);
    if (cursor == std::string::npos) return result;
    ++cursor;
    while (cursor < body.size() && result.size() < 8) {
        while (cursor < body.size() && (body[cursor] == ',' || body[cursor] == ' ' ||
               body[cursor] == '\n' || body[cursor] == '\r')) ++cursor;
        if (cursor >= body.size() || body[cursor] != '{') break;
        const size_t start = cursor;
        int depth = 0;
        bool in_string = false;
        bool escaped = false;
        for (; cursor < body.size(); ++cursor) {
            const char character = body[cursor];
            if (in_string) {
                if (escaped) escaped = false;
                else if (character == 0x5c) escaped = true;
                else if (character == '"') in_string = false;
            } else if (character == '"') in_string = true;
            else if (character == '{') ++depth;
            else if (character == '}' && --depth == 0) { ++cursor; break; }
        }
        if (depth != 0) break;
        const wiliwili::string_view object(body.data() + start, cursor - start);
        DynamicItem item;
        item.id = dynamic_text(object, "id_str", 32);
        if (item.id.empty()) item.id = dynamic_text(object, "dynamic_id", 32);
        if (item.id.empty()) {
            const wiliwili::string_view numeric_id = dynamic_member(object, "id");
            if (numeric_id.size() <= 32) item.id.assign(numeric_id.data(), numeric_id.size());
        }
        const wiliwili::string_view modules = dynamic_member(object, "modules");
        const wiliwili::string_view dynamic = dynamic_module(modules, "module_dynamic");
        const wiliwili::string_view legacy_description = dynamic_module(modules, "module_desc");
        const wiliwili::string_view major = dynamic_member(dynamic, "major");
        const wiliwili::string_view archive = dynamic_member(major, "archive");
        const wiliwili::string_view legacy_archive = dynamic_member(dynamic, "dyn_archive");
        const wiliwili::string_view opus = dynamic_member(major, "opus");
        const wiliwili::string_view pgc = dynamic_member(major, "pgc");
        const wiliwili::string_view live = dynamic_member(major, "live_rcmd");
        const wiliwili::string_view legacy_live = dynamic_member(dynamic_member(dynamic, "dyn_live_rcmd"), "card_info");
        const wiliwili::string_view legacy_live_info = dynamic_member(legacy_live, "live_play_info");
        const wiliwili::string_view description = dynamic_member(dynamic, "desc");
        const wiliwili::string_view opus_summary = dynamic_member(opus, "summary");
        item.bvid = dynamic_text(archive, "bvid", 24);
        if (item.bvid.empty()) item.bvid = dynamic_text(legacy_archive, "bvid", 24);
        if (item.bvid.empty()) item.bvid = dynamic_text(object, "bvid", 24);
        item.title = dynamic_text(archive, "title", 192);
        if (item.title.empty()) item.title = dynamic_text(legacy_archive, "title", 192);
        if (item.title.empty()) item.title = dynamic_text(opus, "title", 192);
        if (item.title.empty()) item.title = dynamic_text(pgc, "title", 192);
        if (item.title.empty()) item.title = dynamic_text(live, "title", 192);
        if (item.title.empty()) item.title = dynamic_text(legacy_live_info, "title", 192);
        if (item.title.empty()) item.title = dynamic_text(description, "text", 192);
        if (item.title.empty()) item.title = dynamic_text(legacy_description, "text", 192);
        if (item.title.empty()) item.title = dynamic_text(opus_summary, "text", 192);
        if (item.title.empty()) item.title = dynamic_text(object, "title", 192);
        item.summary = dynamic_text(archive, "desc", 120);
        if (item.summary.empty()) item.summary = dynamic_text(legacy_archive, "desc", 120);
        if (item.summary.empty()) item.summary = dynamic_text(description, "text", 120);
        if (item.summary.empty()) item.summary = dynamic_text(legacy_description, "text", 120);
        if (item.summary.empty()) item.summary = dynamic_text(opus_summary, "text", 120);
        if (item.summary.empty()) item.summary = dynamic_text(opus, "summary", 120);
        if (item.title.empty()) {
            wiliwili::string_view original_modules = dynamic_member(dynamic_member(object, "orig"), "modules");
            if (original_modules.empty()) {
                const wiliwili::string_view forwarded = dynamic_member(dynamic_member(dynamic, "dyn_forward"), "item");
                original_modules = dynamic_member(forwarded, "modules");
            }
            const wiliwili::string_view original_dynamic = dynamic_module(original_modules, "module_dynamic");
            const wiliwili::string_view original_major = dynamic_member(original_dynamic, "major");
            wiliwili::string_view original_archive = dynamic_member(original_major, "archive");
            if (original_archive.empty()) original_archive = dynamic_member(original_dynamic, "dyn_archive");
            const wiliwili::string_view original_opus = dynamic_member(original_major, "opus");
            const wiliwili::string_view original_summary = dynamic_member(original_opus, "summary");
            const wiliwili::string_view original_description = dynamic_member(original_dynamic, "desc");
            const wiliwili::string_view original_legacy_description = dynamic_module(original_modules, "module_desc");
            item.title = dynamic_text(original_archive, "title", 192);
            if (item.title.empty()) item.title = dynamic_text(original_opus, "title", 192);
            if (item.title.empty()) item.title = dynamic_text(original_description, "text", 192);
            if (item.title.empty()) item.title = dynamic_text(original_legacy_description, "text", 192);
            if (item.title.empty()) item.title = dynamic_text(original_summary, "text", 192);
            if (item.bvid.empty()) item.bvid = dynamic_text(original_archive, "bvid", 24);
            if (item.summary.empty()) item.summary = dynamic_text(original_archive, "desc", 120);
            if (item.summary.empty()) item.summary = dynamic_text(original_summary, "text", 120);
        }
        if (item.summary == item.title) item.summary.clear();
        if (item.title.empty() && !item.id.empty()) item.title = "Dynamic update";
        if (!item.title.empty()) result.push_back(std::move(item));
    }
    return result;
}

void fetch_dynamic_items(bool articles = false, bool append = false) {
    if (bilibili_cookie.empty() || dynamic_fetch_in_flight.exchange(true)) return;
    if (!append) dynamic_fetch_requested = true;
    const int article_page = dynamic_article_page;
    const std::string video_offset = dynamic_video_offset;
    const std::string host_mid = selected_dynamic_mid;
    const uint32_t request_epoch = dynamic_request_epoch;
    std::thread([articles, append, article_page, video_offset, host_mid, request_epoch] {
        std::string body;
        std::string error;
        std::vector<DynamicItem> result;
        const std::string path = articles
            ? "/x/polymer/web-dynamic/desktop/v1/feed/all?page=" + std::to_string(article_page) + "&timezone_offset=-480&platform=web" +
                  (host_mid.empty() ? "" : "&host_mid=" + url_encode(host_mid))
            : "/x/polymer/web-dynamic/desktop/v1/feed/video?page=1&offset=" + url_encode(video_offset) + "&platform=web";
        if (fetch_raw_bilibili_path(path, body, error)) {
            std::string api_code;
            if (json_number_at(body, "code", api_code) && api_code == "0") result = parse_dynamic_items(body);
            else error = "Feed API rejected the request";
        }
        std::string next_offset;
        json_string_at(body, "offset", next_offset);
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_dynamic_items = std::move(result);
            pending_dynamic_append = append;
            pending_dynamic_next_offset = std::move(next_offset);
            pending_dynamic_epoch = request_epoch;
            pending_dynamic_articles = articles;
            pending_dynamic_error = std::move(error);
        }
        dynamic_fetch_in_flight = false;
        dynamic_fetch_complete = true;
    }).detach();
}

std::vector<DynamicUp> parse_dynamic_ups(const std::string& body) {
    std::vector<DynamicUp> result;
    size_t cursor = 0;
    while (result.size() < 6) {
        const size_t marker = body.find("\"uname\":\"", cursor);
        if (marker == std::string::npos) break;
        const size_t start = body.rfind('{', marker);
        const size_t end = body.find('}', marker);
        if (start == std::string::npos || end == std::string::npos) break;
        const std::string object = body.substr(start, std::min(end - start + 1, size_t{512}));
        DynamicUp up;
        json_string_at(object, "uname", up.name);
        limit_dynamic_label(up.name, 96);
        json_number_at(object, "mid", up.mid);
        if (!up.name.empty()) result.push_back(std::move(up));
        cursor = end + 1;
    }
    return result;
}

void fetch_dynamic_ups() {
    if (bilibili_cookie.empty() || dynamic_up_fetch_in_flight.exchange(true)) return;
    dynamic_up_fetch_requested = true;
    std::thread([] {
        std::string body;
        std::string error;
        std::vector<DynamicUp> result;
        if (fetch_raw_bilibili_path("/x/polymer/web-dynamic/v1/uplist", body, error)) result = parse_dynamic_ups(body);
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_dynamic_ups = std::move(result);
        }
        dynamic_up_fetch_in_flight = false;
        dynamic_up_fetch_complete = true;
    }).detach();
}

std::vector<InboxItem> parse_inbox_items(const std::string& body) {
    std::vector<InboxItem> result;
    size_t cursor = 0;
    while (result.size() < 8) {
        const size_t nickname_marker = body.find("\"nickname\":\"", cursor);
        const size_t uname_marker = body.find("\"uname\":\"", cursor);
        const bool use_uname = uname_marker != std::string::npos &&
                               (nickname_marker == std::string::npos || uname_marker < nickname_marker);
        const size_t name_marker = use_uname ? uname_marker : nickname_marker;
        if (name_marker == std::string::npos) break;
        const size_t name_start = name_marker + (use_uname ? 9 : 12);
        const size_t name_end = body.find('"', name_start);
        if (name_end == std::string::npos) break;
        InboxItem item;
        item.sender = body.substr(name_start, name_end - name_start);
        const size_t object_start = body.rfind('{', name_marker);
        const size_t object_end = body.find('}', name_end);
        if (object_start != std::string::npos && object_end != std::string::npos) {
            const std::string object = body.substr(object_start, object_end - object_start + 1);
            json_number_at(object, "talker_id", item.talker_id);
            json_number_at(object, "session_type", item.session_type);
            json_number_at(object, "timestamp", item.timestamp);
            if (item.timestamp.empty()) json_number_at(object, "msg_timestamp", item.timestamp);
            if (item.timestamp.empty()) json_number_at(object, "time", item.timestamp);
            json_number_at(object, "unread_count", item.unread_count);
            if (item.unread_count.empty()) json_number_at(object, "unread", item.unread_count);
            json_number_at(object, "msg_seqno", item.sequence);
            if (item.sequence.empty()) json_number_at(object, "seqno", item.sequence);
        }
        const size_t content_marker = body.find("\"content\":\"", name_end);
        const size_t msg_marker = body.find("\"msg\":\"", name_end);
        const bool use_msg = msg_marker != std::string::npos &&
                             (content_marker == std::string::npos || msg_marker < content_marker);
        const size_t selected_content_marker = use_msg ? msg_marker : content_marker;
        if (selected_content_marker != std::string::npos && selected_content_marker < name_end + 1500) {
            const size_t content_start = selected_content_marker + (use_msg ? 7 : 11);
            const size_t content_end = body.find('"', content_start);
            if (content_end != std::string::npos) item.message = body.substr(content_start, content_end - content_start);
        }
        if (!item.sender.empty() && !item.message.empty()) result.push_back(std::move(item));
        cursor = name_end + 1;
    }
    return result;
}

void fetch_inbox_items(unsigned int tab) {
    if (bilibili_cookie.empty() || inbox_fetch_in_flight.exchange(true)) return;
    if (tab < inbox_fetch_requested.size()) inbox_fetch_requested[tab] = true;
    const char* paths[] = {
        "/x/msgfeed/reply?id=0&reply_time=0",
        "/x/msgfeed/at?id=0&at_time=0",
        "/x/msgfeed/like?id=0&like_time=0",
        "https://api.vc.bilibili.com/session_svr/v1/session_svr/new_sessions?begin_ts=0&mobi_app=web"
    };
    const std::string path = paths[std::min<unsigned int>(tab, 3)];
    std::thread([path] {
        std::string body;
        std::string error;
        std::vector<InboxItem> result;
        if (path.rfind("https://", 0) == 0) {
            std::vector<uint8_t> bytes;
            if (platform_https_get(path, bilibili_headers(), bytes, error)) body.assign(bytes.begin(), bytes.end());
        } else {
            fetch_raw_bilibili_path(path, body, error);
        }
        if (!body.empty()) result = parse_inbox_items(body);
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_inbox_items = std::move(result);
        }
        inbox_fetch_in_flight = false;
        inbox_fetch_complete = true;
    }).detach();
}

void fetch_chat_messages(const InboxItem& session, bool append = false) {
    if (session.talker_id.empty() || chat_fetch_in_flight.exchange(true)) return;
    if (!append) {
        active_chat_talker = session.talker_id;
        active_chat_session_type = session.session_type.empty() ? "1" : session.session_type;
        chat_next_seqno.clear();
    }
    const std::string begin_seqno = append && !chat_next_seqno.empty() ? chat_next_seqno : "0";
    std::thread([talker = active_chat_talker, type = active_chat_session_type, begin_seqno, append] {
        std::vector<uint8_t> bytes;
        std::string error;
        std::vector<InboxItem> result;
        const std::string url = "https://api.vc.bilibili.com/svr_sync/v1/svr_sync/fetch_session_msgs?sender_device_id=1&talker_id=" +
            url_encode(talker) + "&session_type=" + url_encode(type) + "&begin_seqno=" + url_encode(begin_seqno) + "&size=8&mobi_app=web";
        if (platform_https_get(url, bilibili_headers(), bytes, error)) {
            result = parse_inbox_items(std::string(bytes.begin(), bytes.end()));
        }
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_chat_messages = std::move(result);
            pending_chat_append = append;
            pending_chat_next_seqno.clear();
            for (const InboxItem& item : pending_chat_messages) {
                if (item.sequence.empty()) continue;
                if (pending_chat_next_seqno.empty() || std::strtoull(item.sequence.c_str(), nullptr, 10) <
                                                      std::strtoull(pending_chat_next_seqno.c_str(), nullptr, 10))
                    pending_chat_next_seqno = item.sequence;
            }
        }
        chat_fetch_in_flight = false;
        chat_fetch_complete = true;
    }).detach();
}

void load_more_chat_messages(lv_event_t*) {
    if (active_chat_talker.empty() || chat_next_seqno.empty()) return;
    InboxItem session;
    session.talker_id = active_chat_talker;
    session.session_type = active_chat_session_type;
    fetch_chat_messages(session, true);
    if (status_label) lv_label_set_text(status_label, "Inbox: loading older messages...");
}

void show_inbox_page();
void leave_player_page();
void leave_player_page();
void show_main_section(MainSection section);

void select_chat_session(lv_event_t* event) {
    const size_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    if (index >= displayed_inbox_items.size()) return;
    fetch_chat_messages(displayed_inbox_items[index]);
}

void close_chat_thread(lv_event_t*) {
    active_chat_talker.clear();
    active_chat_session_type.clear();
    displayed_chat_messages.clear();
    if (board_preview) show_main_section(MainSection::Inbox);
    else show_inbox_page();
}

void send_chat_message(lv_event_t*) {
    if (!inbox_message_input || active_chat_talker.empty()) return;
    const std::string message = lv_textarea_get_text(inbox_message_input);
    const std::string sender = cookie_field("DedeUserID");
    const std::string csrf = cookie_field("bili_jct");
    if (message.empty() || sender.empty() || csrf.empty()) {
        if (status_label) lv_label_set_text(status_label, "Message requires text and a signed-in Cookie session");
        return;
    }
    const std::string receiver = active_chat_talker;
    std::thread([sender, receiver, csrf, message] {
        std::vector<uint8_t> body;
        std::string error;
        const std::string form = "msg%5Bmsg_type%5D=1&msg%5Bcontent%5D=" + url_encode(message) +
            "&msg%5Bsender_uid%5D=" + url_encode(sender) + "&msg%5Breceiver_id%5D=" + url_encode(receiver) +
            "&msg%5Breceiver_type%5D=1&msg%5Bmsg_status%5D=0&msg%5Btimestamp%5D=" +
            std::to_string(std::time(nullptr)) + "&msg%5Bdev_id%5D=0&msg%5Bnew_face_version%5D=1&csrf=" + url_encode(csrf);
        const std::string url = "https://api.vc.bilibili.com/web_im/v1/web_im/send_msg?w_sender_uid=" +
            url_encode(sender) + "&w_receiver_id=" + url_encode(receiver);
        const bool ok = platform_https_post(url, bilibili_headers(), form, body, error);
        std::string code;
        const bool accepted = ok && json_number_at(std::string(body.begin(), body.end()), "code", code) && code == "0";
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_action_result = accepted ? "Message accepted" : "Message failed";
        }
        if (accepted) refresh_chat_after_action = true;
        action_result_complete = true;
    }).detach();
    lv_textarea_set_text(inbox_message_input, "");
}

std::vector<Video> parse_history_videos(const std::string& body) {
    std::vector<Video> result;
    size_t cursor = 0;
    while (result.size() < 6) {
        const size_t marker = body.find("\"bvid\":\"", cursor);
        if (marker == std::string::npos) break;
        const size_t start = marker + 8;
        const size_t end = body.find('"', start);
        if (end == std::string::npos) break;
        const size_t object_end = body.find("\"view_at\":", end);
        const std::string object = body.substr(marker > 200 ? marker - 200 : 0,
                                                object_end == std::string::npos ? std::string::npos : object_end - (marker > 200 ? marker - 200 : 0));
        Video video;
        video.bvid = body.substr(start, end - start);
        json_string_at(object, "title", video.title);
        json_string_at(object, "cover", video.pic);
        if (video.pic.empty()) json_string_at(object, "pic", video.pic);
        json_string_at(object, "author_name", video.owner);
        if (video.owner.empty()) json_string_at(object, "name", video.owner);
        if (!video.title.empty() && !video.bvid.empty()) result.push_back(std::move(video));
        cursor = end + 1;
    }
    return result;
}

void fetch_history_videos() {
    if (bilibili_cookie.empty() || history_fetch_in_flight.exchange(true)) return;
    mine_fetch_requested[0] = true;
    std::thread([] {
        std::string body;
        std::string error;
        std::vector<Video> result;
        const std::string path = "/x/web-interface/history/cursor?max=0&view_at=0&business=&ps=6";
        if (fetch_raw_bilibili_path(path, body, error)) result = parse_history_videos(body);
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_history_videos = std::move(result);
        }
        history_fetch_in_flight = false;
        history_fetch_complete = true;
    }).detach();
}

void fetch_favorite_videos() {
    if (bilibili_cookie.empty() || favorite_fetch_in_flight.exchange(true)) return;
    mine_fetch_requested[1] = true;
    const std::string mid = cookie_field("DedeUserID");
    std::thread([mid] {
        std::string folders;
        std::string error;
        std::vector<Video> result;
        if (!mid.empty() && fetch_raw_bilibili_path("/x/v3/fav/folder/created/list?up_mid=" + url_encode(mid) + "&jsonp=jsonp", folders, error)) {
            std::string media_id;
            json_number_at(folders, "media_id", media_id);
            if (media_id.empty()) json_number_at(folders, "id", media_id);
            if (!media_id.empty()) {
                std::string resources;
                if (fetch_raw_bilibili_path("/x/v3/fav/resource/list?media_id=" + url_encode(media_id) + "&ps=6&pn=1", resources, error)) {
                    result = parse_video_list_loose(resources);
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_favorite_videos = std::move(result);
        }
        favorite_fetch_in_flight = false;
        favorite_fetch_complete = true;
    }).detach();
}

void fetch_later_videos() {
    if (bilibili_cookie.empty() || later_fetch_in_flight.exchange(true)) return;
    mine_fetch_requested[5] = true;
    std::thread([] {
        std::string body;
        std::string error;
        std::vector<Video> result;
        if (fetch_raw_bilibili_path("/x/v2/history/toview/web", body, error)) result = parse_video_list_loose(body);
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_later_videos = std::move(result);
        }
        later_fetch_in_flight = false;
        later_fetch_complete = true;
    }).detach();
}

void remove_later_video(lv_event_t* event) {
    lv_event_stop_bubbling(event);
    const size_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    if (index >= displayed_later_videos.size()) return;
    const Video video = displayed_later_videos[index];
    const std::string csrf = cookie_field("bili_jct");
    if (video.aid.empty() || csrf.empty()) {
        if (status_label) lv_label_set_text(status_label, "Remove requires a signed-in Cookie session");
        return;
    }
    std::thread([aid = video.aid, csrf] {
        std::vector<uint8_t> body;
        std::string error;
        const bool ok = platform_https_post("https://api.bilibili.com/x/v2/history/toview/del",
                                            bilibili_headers(),
                                            "aid=" + url_encode(aid) + "&csrf=" + url_encode(csrf), body, error);
        std::string code;
        const bool accepted = ok && json_number_at(std::string(body.begin(), body.end()), "code", code) && code == "0";
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_action_result = accepted ? "Removed from watch later" : "Watch later removal failed";
        }
        if (accepted) refresh_later_after_action = true;
        action_result_complete = true;
    }).detach();
    if (status_label) lv_label_set_text(status_label, "Removing from watch later...");
}

std::vector<std::string> parse_title_list(const std::string& body) {
    std::vector<std::string> result;
    size_t cursor = 0;
    while (result.size() < 6) {
        const size_t marker = body.find("\"title\":\"", cursor);
        if (marker == std::string::npos) break;
        const size_t start = marker + 9;
        const size_t end = body.find('"', start);
        if (end == std::string::npos) break;
        const std::string title = body.substr(start, end - start);
        if (!title.empty()) result.push_back(title);
        cursor = end + 1;
    }
    return result;
}

void fetch_subscriptions() {
    if (bilibili_cookie.empty() || subscription_fetch_in_flight.exchange(true)) return;
    mine_fetch_requested[2] = true;
    const std::string mid = cookie_field("DedeUserID");
    std::thread([mid] {
        std::string body;
        std::string error;
        std::vector<std::string> result;
        if (!mid.empty() && fetch_raw_bilibili_path("/x/v3/fav/folder/collected/list?up_mid=" +
                                                     url_encode(mid) + "&platform=web&ps=6&pn=1", body, error)) {
            result = parse_title_list(body);
        }
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_subscription_titles = std::move(result);
        }
        subscription_fetch_in_flight = false;
        subscription_fetch_complete = true;
    }).detach();
}

void fetch_bangumi_titles() {
    if (bilibili_cookie.empty() || bangumi_fetch_in_flight.exchange(true)) return;
    mine_fetch_requested[3] = true;
    const std::string mid = cookie_field("DedeUserID");
    std::thread([mid] {
        std::string body;
        std::string error;
        std::vector<std::string> result;
        if (!mid.empty() && fetch_raw_bilibili_path("/x/space/bangumi/follow/list?vmid=" +
                                                     url_encode(mid) + "&ps=6&pn=1&type=1&platform=web&follow_status=0",
                                                     body, error)) {
            result = parse_title_list(body);
        }
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_bangumi_titles = std::move(result);
        }
        bangumi_fetch_in_flight = false;
        bangumi_fetch_complete = true;
    }).detach();
}

void fetch_series_titles() {
    if (bilibili_cookie.empty() || series_fetch_in_flight.exchange(true)) return;
    mine_fetch_requested[4] = true;
    const std::string mid = cookie_field("DedeUserID");
    std::thread([mid] {
        std::string body;
        std::string error;
        std::vector<std::string> result;
        if (!mid.empty() && fetch_raw_bilibili_path("/x/space/bangumi/follow/list?vmid=" +
                                                     url_encode(mid) + "&ps=6&pn=1&type=2&platform=web&follow_status=0",
                                                     body, error)) {
            result = parse_title_list(body);
        }
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_series_titles = std::move(result);
        }
        series_fetch_in_flight = false;
        series_fetch_complete = true;
    }).detach();
}

void probe_account();

void fetch_qr_login() {
    if (qr_fetch_in_flight.exchange(true)) return;
    std::thread([] {
        std::string body;
        std::string error;
        std::string url;
        std::string key;
        const std::string nonce = std::to_string(static_cast<unsigned long long>(std::time(nullptr))) +
                                  std::to_string(static_cast<unsigned long long>(std::rand()));
        const std::string device_uuid = nonce + "-wiliwili-lvgl-infoc";
        const std::string device_buvid = nonce + "000000000000000000000000";
        const std::string qr_headers = std::string(kBilibiliHeaders) +
            "Cookie: appkey=aa1e74ee4874176e; mobi_app=pc_electron; device=mac; innersign=0; "
            "device_id=windows_lvgl; device_name=wiliwili%20LVGL; _uuid=" + device_uuid +
            "; buvid3=" + device_buvid + "\r\n";
        const std::string path = "https://passport.bilibili.com/x/passport-login/web/qrcode/generate?source=main_electron_pc";
        std::vector<uint8_t> bytes;
        std::string generate_response_headers;
        if (platform_https_get(path, qr_headers, bytes, error, &generate_response_headers)) {
            body.assign(bytes.begin(), bytes.end());
            json_string_at(body, "url", url);
            json_string_at(body, "qrcode_key", key);
        }
        const bool token_ready = !url.empty() && !key.empty();
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            qr_login_url = url;
            qr_login_key = key;
        }
        qr_fetch_in_flight = false;
        qr_fetch_complete = true;
        std::ofstream qr_debug(lvgl_config_path() + ".qr-poll.log", std::ios::trunc);
        if (!token_ready) {
            std::string api_code;
            json_number_at(body, "code", api_code);
            if (qr_debug) qr_debug << "generate url=" << (!url.empty()) << " key=" << (!key.empty())
                                   << " http_error=" << (!error.empty()) << " api_code=" << api_code << "\n";
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_qr_status = !error.empty() ? "QR: " + error : !api_code.empty() && api_code != "0" ?
                                "QR: generate rejected (code " + api_code + ")" : "QR: code unavailable; retry Show QR";
            qr_status_complete = true;
            return;
        }
        std::string generate_cookies;
        size_t generate_cursor = 0;
        while ((generate_cursor = generate_response_headers.find("Set-Cookie:", generate_cursor)) != std::string::npos) {
            size_t start = generate_cursor + 11;
            while (start < generate_response_headers.size() && (generate_response_headers[start] == ' ' || generate_response_headers[start] == '\t')) ++start;
            const size_t end = generate_response_headers.find(';', start);
            if (!generate_cookies.empty()) generate_cookies += "; ";
            generate_cookies += generate_response_headers.substr(start, end == std::string::npos ? std::string::npos : end - start);
            generate_cursor = end == std::string::npos ? generate_response_headers.size() : end + 1;
        }
        std::string poll_headers = qr_headers;
        if (!generate_cookies.empty()) {
            const size_t cookie = poll_headers.find("Cookie: ");
            if (cookie != std::string::npos) {
                const size_t line_end = poll_headers.find("\r\n", cookie);
                poll_headers.insert(line_end, "; " + generate_cookies);
            }
        }
        bool login_completed = false;
        if (qr_debug) qr_debug << "generate url=1 key=1\n" << std::flush;
        for (int attempt = 0; attempt < 30 && !login_completed; ++attempt) {
            platform_delay(3000);
            std::vector<uint8_t> poll_bytes;
            std::string response_headers;
            std::string poll_error;
            const std::string poll_url = "https://passport.bilibili.com/x/passport-login/web/qrcode/poll?qrcode_key=" +
                                         url_encode(key) + "&source=main_electron_pc";
            if (!platform_https_get(poll_url, poll_headers, poll_bytes, poll_error, &response_headers)) {
                if (qr_debug) qr_debug << "poll " << attempt + 1 << " request_failed\n" << std::flush;
                std::lock_guard<std::mutex> lock(data_mutex);
                pending_qr_status = "QR: poll request failed (retrying)";
                qr_status_complete = true;
                continue;
            }
            const std::string poll_body(poll_bytes.begin(), poll_bytes.end());
            std::string api_code;
            json_number_at(poll_body, "code", api_code);
            const size_t data = poll_body.find("\"data\":{");
            std::string login_code;
            if (data != std::string::npos) json_number_at(poll_body.substr(data), "code", login_code);
            // The envelope code only reports API transport success. QR state
            // lives in data.code (86101/86090 while pending, 0 when confirmed).
            if (api_code != "0" || login_code != "0") {
                if (qr_debug) qr_debug << "poll " << attempt + 1 << " api=" << api_code
                                       << " state=" << login_code << "\n" << std::flush;
                std::lock_guard<std::mutex> lock(data_mutex);
                const std::string observed = login_code.empty() ? api_code : login_code;
                pending_qr_status = login_code == "86101" ? "QR poll " + std::to_string(attempt + 1) + ": waiting for scan" :
                                    login_code == "86090" ? "QR poll " + std::to_string(attempt + 1) + ": confirm on phone" :
                                    login_code == "86038" ? "QR: code expired; request a new code" :
                                    "QR poll " + std::to_string(attempt + 1) + ": status " + observed;
                qr_status_complete = true;
                continue;
            }
            std::string merged;
            size_t cursor = 0;
            while ((cursor = response_headers.find("Set-Cookie:", cursor)) != std::string::npos) {
                size_t start = cursor + 11;
                while (start < response_headers.size() && (response_headers[start] == ' ' || response_headers[start] == '\t')) ++start;
                const size_t end = response_headers.find(';', start);
                if (!merged.empty()) merged += "; ";
                merged += response_headers.substr(start, end == std::string::npos ? std::string::npos : end - start);
                cursor = end == std::string::npos ? response_headers.size() : end + 1;
            }
            if (!merged.empty()) {
                if (qr_debug) qr_debug << "poll " << attempt + 1 << " success session_headers=1\n";
                merged += "; _uuid=" + device_uuid + "; buvid3=" + device_buvid;
                bilibili_cookie = std::move(merged);
                save_cookie();
                login_completed = true;
                {
                    std::lock_guard<std::mutex> lock(data_mutex);
                    pending_qr_status = "QR: session received; verifying account";
                    qr_status_complete = true;
                }
                probe_account();
            } else {
                if (qr_debug) qr_debug << "poll " << attempt + 1 << " success session_headers=0\n";
                std::lock_guard<std::mutex> lock(data_mutex);
                pending_qr_status = "QR: confirmed but no session cookie returned";
                qr_status_complete = true;
            }
            break;
        }
    }).detach();
}

void probe_account() {
    if (bilibili_cookie.empty() || account_fetch_in_flight.exchange(true)) return;
    std::thread([] {
        std::string body;
        std::string error;
        std::string name;
        std::array<std::string, 4> stats{};
        if (fetch_raw_bilibili_path("/x/web-interface/nav", body, error)) {
            json_string_at(body, "uname", name);
        }
        if (!name.empty() && fetch_raw_bilibili_path("/x/space/myinfo", body, error)) {
            json_number_at(body, "following", stats[1]);
            json_number_at(body, "follower", stats[2]);
            json_number_at(body, "dynamic", stats[0]);
            json_number_at(body, "coins", stats[3]);
        }
        std::lock_guard<std::mutex> lock(data_mutex);
        account_name = std::move(name);
        account_stats = std::move(stats);
        account_fetch_in_flight = false;
        account_fetch_complete = true;
    }).detach();
}

bool fetch_thumbnail(const std::string& url, std::vector<uint8_t>& bytes, size_t byte_limit) {
    constexpr const char kHttpPrefix[] = "http://";
    constexpr const char kHttpsPrefix[] = "https://";
    std::string address = url;
    if (address.rfind("//", 0) == 0) address = std::string("https:") + address;
    if (address.rfind(kHttpPrefix, 0) == 0) address.replace(0, sizeof(kHttpPrefix) - 1, kHttpsPrefix);
    if (address.rfind(kHttpsPrefix, 0) != 0) return false;
    const size_t host_end = address.find('/', sizeof(kHttpsPrefix) - 1);
    if (host_end != std::string::npos) {
        const std::string host = address.substr(sizeof(kHttpsPrefix) - 1, host_end - (sizeof(kHttpsPrefix) - 1));
        if ((host == "i0.hdslb.com" || host == "i1.hdslb.com" || host == "i2.hdslb.com") &&
            address.find('@', host_end) == std::string::npos) {
            const size_t query_start = address.find('?', host_end);
            address.insert(query_start == std::string::npos ? address.size() : query_start, "@256w_144h_1c.jpg");
        }
    }
    std::string error;
    std::vector<uint8_t>().swap(bytes);
    bytes.reserve(byte_limit);
    if (platform_https_get(address, bilibili_headers(), bytes, error, nullptr, byte_limit)) {
        bytes.shrink_to_fit();
        return true;
    }
    bytes.clear();
    bytes.shrink_to_fit();
    return false;
}

void prepare_thumbnails(std::vector<Video>& videos) {
    lv_img_cache_invalidate_src(nullptr);
    thumbnail_pixels.clear();
    thumbnail_descriptors.clear();
    thumbnail_pixels.reserve(kMaxFeedVideos);
    thumbnail_descriptors.reserve(kMaxFeedVideos);
    thumbnail_pixels.resize(videos.size());
    thumbnail_descriptors.resize(videos.size());
    for (size_t i = 0; i < videos.size(); ++i) {
        if (videos[i].image_bytes.empty()) continue;
        bool has_alpha = false;
        if (!platform_decode_thumbnail_lvgl(videos[i].image_bytes, 186, 104, thumbnail_pixels[i], &has_alpha)) {
            videos[i].image_bytes.clear();
            videos[i].image_bytes.shrink_to_fit();
            continue;
        }
        lv_img_dsc_t& descriptor = thumbnail_descriptors[i];
        descriptor.header.always_zero = 0;
        descriptor.header.w = 186;
        descriptor.header.h = 104;
        descriptor.header.cf = has_alpha ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR;
        descriptor.data_size = static_cast<uint32_t>(thumbnail_pixels[i].size());
        descriptor.data = thumbnail_pixels[i].data();
        // LVGL now owns the decoded pixels. Drop the compressed network copy
        // so a board feed refresh does not retain two representations of each
        // cover in the 64 MiB process.
        videos[i].image_bytes.clear();
        videos[i].image_bytes.shrink_to_fit();
    }
}

void append_thumbnails(std::vector<Video>& videos) {
    thumbnail_pixels.reserve(kMaxFeedVideos);
    thumbnail_descriptors.reserve(kMaxFeedVideos);
    for (Video& video : videos) {
        thumbnail_pixels.emplace_back();
        thumbnail_descriptors.emplace_back();
        bool has_alpha = false;
        if (!video.image_bytes.empty() &&
            platform_decode_thumbnail_lvgl(video.image_bytes, 186, 104, thumbnail_pixels.back(), &has_alpha)) {
            lv_img_dsc_t& descriptor = thumbnail_descriptors.back();
            descriptor.header.always_zero = 0;
            descriptor.header.w = 186;
            descriptor.header.h = 104;
            descriptor.header.cf = has_alpha ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR;
            descriptor.data_size = static_cast<uint32_t>(thumbnail_pixels.back().size());
            descriptor.data = thumbnail_pixels.back().data();
        }
        video.image_bytes.clear();
        video.image_bytes.shrink_to_fit();
    }
}

void show_recommend_page(const std::vector<Video>& videos, const std::string& error);
void show_board_home(const std::vector<Video>& videos, const std::string& error);
void show_search_results_page(const std::vector<Video>& videos, const std::string& error);
void start_fetch();
void show_search_history_page();
void show_search_hots_page();
void fetch_search_hots();
void select_hot_query(lv_event_t* event);
void load_more_video_comments(lv_event_t* event);
void open_media_search_result(lv_event_t* event);
void fetch_media_search(const char* category);
void show_media_search_page();
void fetch_season_detail(const std::string& season_id);
void show_season_detail_page();
void open_season_episode(lv_event_t* event);
void return_to_media_search(lv_event_t* event);
void select_search_category(lv_event_t* event);
void show_player_page(size_t video_index);
void cycle_playback_speed(lv_event_t* event);
void cycle_danmaku_speed(lv_event_t* event);
void toggle_player_playback(lv_event_t* event);
void request_video_detail(size_t video_index, size_t part_index = 0);
void select_next_video_part(lv_event_t* event);
void select_player_side_panel(lv_event_t* event);
void select_player_video_part(lv_event_t* event);
void open_related_video(lv_event_t* event);
void show_main_section(MainSection section);
void select_main_section(lv_event_t* event);
void update_shell_for_section(MainSection section);
void show_dynamic_page();
void show_mine_page();
void show_settings_page();
void show_inbox_page();

void refresh_recommend(lv_event_t*) {
    if (fetch_in_flight.exchange(true)) return;
    ++navigation_epoch;
    if (player_page_visible || player_playing) leave_player_page();
    detail_visible = false;
    season_page_visible = false;
    main_section = MainSection::Home;
    update_shell_for_section(main_section);
    if (nav_rail) lv_obj_clear_flag(nav_rail, LV_OBJ_FLAG_HIDDEN);
    if (content) {
        lv_obj_del(content);
        content = nullptr;
    }
    feed_page = 1;
    feed_append = false;
    lv_label_set_text(status_label, "Refreshing Bilibili Home Recommend...");
    start_fetch();
}

void update_feed_tab_style() {
    if (!recommend_tab || !popular_tab) return;
    lv_obj_set_style_bg_color(recommend_tab,
                              ui_color(popular_selected ? 0x374152 : 0x2877a9), 0);
    lv_obj_set_style_bg_color(popular_tab,
                              ui_color(popular_selected ? 0x2877a9 : 0x374152), 0);
}

void select_recommend(lv_event_t*) {
    main_section = MainSection::Home;
    update_shell_for_section(main_section);
    popular_selected = false;
    feed_mode = FeedMode::Recommend;
    feed_page = 1;
    feed_append = false;
    update_feed_tab_style();
    refresh_recommend(nullptr);
}

void select_popular(lv_event_t*) {
    main_section = MainSection::Home;
    update_shell_for_section(main_section);
    popular_selected = true;
    feed_mode = FeedMode::Popular;
    feed_page = 1;
    feed_append = false;
    update_feed_tab_style();
    refresh_recommend(nullptr);
}

void submit_search(lv_event_t*) {
    if (!search_input || fetch_in_flight.exchange(true)) return;
    search_term = lv_textarea_get_text(search_input);
    if (search_term.empty()) {
        fetch_in_flight = false;
        show_search_history_page();
        return;
    }
    add_search_history(search_term);
    search_focused = false;
    active_text_input = nullptr;
    main_section = MainSection::Home;
    feed_mode = FeedMode::Search;
    feed_page = 1;
    feed_append = false;
    popular_selected = false;
    update_feed_tab_style();
    lv_label_set_text(status_label, "Searching Bilibili videos...");
    start_fetch();
}

void load_more_feed(lv_event_t*) {
    if (displayed_videos.size() >= kMaxFeedVideos) {
        if (status_label) lv_label_set_text(status_label, "Showing first 32 results; refine your search for more");
        return;
    }
    if (fetch_in_flight.exchange(true)) return;
    ++feed_page;
    feed_append = true;
    if (status_label) lv_label_set_text(status_label, "Loading next page...");
    start_fetch();
}

void previous_feed_page(lv_event_t*) {
    if (feed_page <= 1 || fetch_in_flight.exchange(true)) return;
    --feed_page;
    feed_append = false;
    if (status_label) lv_label_set_text(status_label, "Loading previous page...");
    start_fetch();
}

void next_feed_page(lv_event_t*) {
    if (fetch_in_flight.exchange(true)) return;
    ++feed_page;
    feed_append = false;
    if (status_label) lv_label_set_text(status_label, "Loading next page...");
    start_fetch();
}

void select_history_query(lv_event_t* event) {
    const size_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    if (index >= search_history.size() || !search_input) return;
    lv_textarea_set_text(search_input, search_history[index].c_str());
    submit_search(nullptr);
}

void clear_search_history(lv_event_t*) {
    search_history.clear();
    save_search_history();
    show_search_history_page();
}

void select_hot_query(lv_event_t* event) {
    const size_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    if (index >= displayed_hot_terms.size() || !search_input) return;
    lv_textarea_set_text(search_input, displayed_hot_terms[index].keyword.c_str());
    submit_search(nullptr);
}

void show_search_hots_page() {
    if (content) lv_obj_del(content);
    detail_visible = false;
    content = lv_obj_create(lv_scr_act());
    const int width = board_preview ? 720 : 1120;
    const int height = board_preview ? 382 : 590;
    lv_obj_set_size(content, width, height);
    lv_obj_set_pos(content, board_preview ? 72 : 130, board_preview ? 86 : 110);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    // The feed is paged (eight cards per page), not a scroll view.
    lock_static_canvas(content);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(content, board_preview ? 10 : 14, 0);
    lv_obj_set_style_pad_row(content, board_preview ? 10 : 14, 0);
    lock_static_canvas(content);
    lv_label_set_text(status_label, "Bilibili Trending Searches");
    for (size_t index = 0; index < displayed_hot_terms.size(); ++index) {
        const HotTerm& term = displayed_hot_terms[index];
        lv_obj_t* item = lv_btn_create(content);
        lv_obj_set_size(item, board_preview ? 350 : 348, board_preview ? 48 : 56);
        lv_obj_set_style_bg_color(item, ui_color(0x202735), 0);
        lv_obj_set_style_radius(item, 5, 0);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_add_event_cb(item, select_hot_query, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(index)));
        const std::string label = std::to_string(index + 1) + ". " + term.keyword;
        set_label(item, label.c_str(), board_preview ? 13 : 16, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 14, 0);
        if (!term.heat.empty()) set_label(item, term.heat.c_str(), board_preview ? 11 : 13,
                                          ui_color(0xaeb8c6), LV_ALIGN_RIGHT_MID, -12, 0);
    }
}

void fetch_search_hots() {
    if (hot_fetch_in_flight.exchange(true)) return;
    const uint32_t request_epoch = ++navigation_epoch;
    if (status_label) lv_label_set_text(status_label, "Loading Bilibili trending searches...");
    std::thread([request_epoch] {
        std::string body;
        std::string error;
        std::vector<HotTerm> terms;
        if (fetch_raw_bilibili_path("/x/web-interface/search/square?limit=8", body, error)) {
            terms = parse_hot_terms(body);
        }
        std::lock_guard<std::mutex> lock(data_mutex);
        pending_hot_terms = std::move(terms);
        pending_hot_epoch = request_epoch;
        hot_fetch_in_flight = false;
        hot_fetch_complete = true;
    }).detach();
}

void show_media_search_page() {
    if (content) lv_obj_del(content);
    detail_visible = false;
    content = lv_obj_create(lv_scr_act());
    const int width = board_preview ? 720 : 1120;
    const int height = board_preview ? 382 : 590;
    lv_obj_set_size(content, width, height);
    lv_obj_set_pos(content, board_preview ? 72 : 130, board_preview ? 86 : 110);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(content, board_preview ? 10 : 14, 0);
    lv_obj_set_style_pad_row(content, board_preview ? 10 : 14, 0);
    lock_static_canvas(content);
    const std::string heading = std::string(media_search_category == "media_bangumi" ? "Bangumi" : "Cinema") +
                                " Search: " + search_term;
    lv_label_set_text(status_label, heading.c_str());
    for (size_t index = 0; index < displayed_media_results.size(); ++index) {
        const MediaSearchItem& result = displayed_media_results[index];
        lv_obj_t* item = lv_btn_create(content);
        lv_obj_set_size(item, board_preview ? 350 : 348, board_preview ? 76 : 90);
        lv_obj_set_style_bg_color(item, ui_color(0x202735), 0);
        lv_obj_set_style_radius(item, 5, 0);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_add_event_cb(item, open_media_search_result, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(index)));
        lv_obj_t* title = lv_label_create(item);
        lv_label_set_text(title, result.title.c_str());
        lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(title, board_preview ? 326 : 322);
        lv_obj_set_style_text_font(title, ui_font(), 0);
        lv_obj_set_style_text_color(title, ui_color(0xf3f5f8), 0);
        lv_obj_set_pos(title, 12, 12);
        if (!result.subtitle.empty()) set_label(item, result.subtitle.c_str(), board_preview ? 12 : 14,
                                                ui_color(0xaeb8c6), LV_ALIGN_BOTTOM_LEFT, 12, -10);
    }
}

void open_media_search_result(lv_event_t* event) {
    const size_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    if (index >= displayed_media_results.size() || displayed_media_results[index].id.empty()) return;
    fetch_season_detail(displayed_media_results[index].id);
}

void return_to_media_search(lv_event_t*) {
    detail_visible = false;
    season_page_visible = false;
    show_media_search_page();
}

void open_season_episode(lv_event_t* event) {
    const size_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    if (index >= displayed_season.episodes.size()) return;
    const SeasonEpisode& episode = displayed_season.episodes[index];
    if (episode.bvid.empty()) return;
    Video video;
    video.bvid = episode.bvid;
    video.cid = episode.cid;
    video.title = displayed_season.title + (episode.title.empty() ? std::string{} : " - " + episode.title);
    displayed_videos.clear();
    displayed_videos.push_back(std::move(video));
    active_video_index = 0;
    active_part_index = 0;
    detail_return_to_season = true;
    detail_return_to_dynamic = false;
    request_video_detail(0);
}

void show_season_detail_page() {
    if (content) lv_obj_del(content);
    detail_visible = true;
    season_page_visible = true;
    content = lv_obj_create(lv_scr_act());
    const int width = board_preview ? 720 : 1120;
    const int height = board_preview ? 382 : 590;
    lv_obj_set_size(content, width, height);
    lv_obj_set_pos(content, board_preview ? 72 : 130, board_preview ? 86 : 110);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lock_static_canvas(content);

    lv_obj_t* back = lv_btn_create(content);
    lv_obj_set_size(back, board_preview ? 76 : 96, 36);
    lv_obj_set_pos(back, 0, 0);
    lv_obj_set_style_bg_color(back, ui_color(0x374152), 0);
    lv_obj_set_style_radius(back, 5, 0);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_add_event_cb(back, return_to_media_search, LV_EVENT_CLICKED, nullptr);
    set_label(back, "Back", board_preview ? 12 : 14, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);

    const std::string title = displayed_season.title.empty() ? "Season" : displayed_season.title;
    set_label(content, title.c_str(), board_preview ? 18 : 24, ui_color(0xf3f5f8), LV_ALIGN_TOP_LEFT,
              board_preview ? 88 : 112, 6);
    if (!displayed_season.subtitle.empty()) {
        set_label(content, displayed_season.subtitle.c_str(), board_preview ? 12 : 14, ui_color(0x8fd3ff),
                  LV_ALIGN_TOP_LEFT, board_preview ? 88 : 112, board_preview ? 32 : 40);
    }
    if (!displayed_season.description.empty()) {
        lv_obj_t* description = lv_label_create(content);
        lv_label_set_text(description, displayed_season.description.c_str());
        lv_label_set_long_mode(description, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(description, width - 24);
        lv_obj_set_style_text_font(description, ui_font(), 0);
        lv_obj_set_style_text_color(description, ui_color(0xaeb8c6), 0);
        lv_obj_set_pos(description, 0, board_preview ? 62 : 72);
    }

    lv_obj_t* episode_pane = lv_obj_create(content);
    lv_obj_set_size(episode_pane, width, height - (board_preview ? 108 : 126));
    lv_obj_set_pos(episode_pane, 0, board_preview ? 104 : 122);
    lv_obj_set_style_bg_color(episode_pane, ui_color(0x1d2330), 0);
    lv_obj_set_style_radius(episode_pane, 6, 0);
    lv_obj_set_style_border_width(episode_pane, 0, 0);
    lv_obj_set_style_pad_all(episode_pane, board_preview ? 8 : 12, 0);
    lv_obj_set_flex_flow(episode_pane, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(episode_pane, board_preview ? 6 : 8, 0);
    lv_obj_set_style_pad_column(episode_pane, board_preview ? 6 : 8, 0);
    if (displayed_season.episodes.empty()) {
        set_label(episode_pane, "No playable episodes were returned.", board_preview ? 13 : 16,
                  ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 0);
    } else {
        for (size_t index = 0; index < displayed_season.episodes.size(); ++index) {
            const SeasonEpisode& episode = displayed_season.episodes[index];
            lv_obj_t* button = lv_btn_create(episode_pane);
            lv_obj_set_size(button, board_preview ? 338 : 258, board_preview ? 38 : 44);
            lv_obj_set_style_bg_color(button, ui_color(0x202735), 0);
            lv_obj_set_style_radius(button, 4, 0);
            lv_obj_set_style_border_width(button, 0, 0);
            lv_obj_add_event_cb(button, open_season_episode, LV_EVENT_CLICKED,
                                reinterpret_cast<void*>(static_cast<uintptr_t>(index)));
            std::string label = std::to_string(index + 1) + ". " + episode.title;
            if (!episode.badge.empty()) label += " [" + episode.badge + "]";
            set_label(button, label.c_str(), board_preview ? 12 : 14, ui_color(0xf3f5f8),
                      LV_ALIGN_LEFT_MID, 10, 0);
        }
    }
    const std::string status = "Season: " + std::to_string(displayed_season.episodes.size()) + " episodes";
    if (status_label) lv_label_set_text(status_label, status.c_str());
}

void fetch_season_detail(const std::string& season_id) {
    if (season_id.empty() || season_fetch_in_flight.exchange(true)) return;
    const uint32_t request_epoch = ++navigation_epoch;
    if (status_label) lv_label_set_text(status_label, "Loading season details...");
    std::thread([season_id, request_epoch] {
        std::string body;
        std::string error;
        SeasonDetail detail;
        if (fetch_raw_bilibili_path("/pgc/view/web/season?season_id=" + url_encode(season_id), body, error)) {
            detail = parse_season_detail(body, season_id);
        }
        std::lock_guard<std::mutex> lock(data_mutex);
        pending_season = std::move(detail);
        pending_season_epoch = request_epoch;
        season_fetch_in_flight = false;
        season_fetch_complete = true;
    }).detach();
}

void fetch_media_search(const char* category) {
    if (search_term.empty() || media_fetch_in_flight.exchange(true)) return;
    const uint32_t request_epoch = ++navigation_epoch;
    media_search_category = category;
    if (status_label) lv_label_set_text(status_label, "Loading Bilibili media search...");
    const std::string query = search_term;
    std::thread([query, category, request_epoch] {
        std::string body;
        std::string error;
        std::vector<MediaSearchItem> results;
        const std::string path = "/x/web-interface/search/type?search_type=" + std::string(category) +
                                 "&page=1&page_size=8&keyword=" + url_encode(query);
        if (fetch_raw_bilibili_path(path, body, error)) results = parse_media_search(body);
        std::lock_guard<std::mutex> lock(data_mutex);
        pending_media_results = std::move(results);
        pending_media_epoch = request_epoch;
        media_fetch_in_flight = false;
        media_fetch_complete = true;
    }).detach();
}

void show_search_history_page() {
    ++navigation_epoch;
    for (lv_obj_t* control : home_header_controls) {
        if (control) lv_obj_add_flag(control, LV_OBJ_FLAG_HIDDEN);
    }
    if (content) lv_obj_del(content);
    detail_visible = false;
    content = lv_obj_create(lv_scr_act());
    const int content_width = board_preview ? 720 : 1120;
    const int content_height = board_preview ? 382 : 590;
    const int item_width = board_preview ? 348 : 348;
    const int item_height = board_preview ? 42 : 50;
    const int font_size = board_preview ? 13 : 16;
    lv_obj_set_size(content, content_width, content_height);
    lv_obj_set_pos(content, board_preview ? 72 : 130, board_preview ? 100 : 110);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(content, board_preview ? 12 : 14, 0);
    lv_obj_set_style_pad_row(content, board_preview ? 10 : 14, 0);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    if (top_title) lv_label_set_text(top_title, "Search");
    lv_label_set_text(status_label, "Search history");
    if (search_history.empty()) {
        set_label(content, "No search history", board_preview ? 15 : 18, ui_color(0xaeb8c6),
                  LV_ALIGN_TOP_LEFT, 0, board_preview ? 8 : 12);
        return;
    }
    // ProgramConfig exposes newest-first; the stored list remains chronological
    // so duplicate removal and the 50-item cap match config_helper.cpp.
    for (size_t reverse = search_history.size(); reverse > 0; --reverse) {
        const size_t index = reverse - 1;
        lv_obj_t* item = lv_btn_create(content);
        lv_obj_set_size(item, item_width, item_height);
        lv_obj_set_style_bg_color(item, ui_color(0x202735), 0);
        lv_obj_set_style_radius(item, 5, 0);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_add_event_cb(item, select_history_query, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(index)));
        lv_obj_t* label = lv_label_create(item);
        lv_label_set_text(label, search_history[index].c_str());
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_set_size(label, item_width - 32, 24);
        lv_obj_set_style_text_font(label, ui_font(), 0);
        lv_obj_set_style_text_color(label, ui_color(0xf3f5f8), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 16, 0);
    }
    lv_obj_t* clear = lv_btn_create(content);
    lv_obj_set_size(clear, item_width, item_height);
    lv_obj_set_style_bg_color(clear, ui_color(0x8d3b45), 0);
    lv_obj_set_style_radius(clear, 5, 0);
    lv_obj_set_style_border_width(clear, 0, 0);
    lv_obj_add_event_cb(clear, clear_search_history, LV_EVENT_CLICKED, nullptr);
    set_label(clear, "Clear search history", font_size, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
}

void focus_search(lv_event_t*) {
    search_focused = true;
    active_text_input = search_input;
    active_text_target = TextInputTarget::Search;
}

void unfocus_search(lv_event_t*) {
    search_focused = false;
    if (active_text_input == search_input) active_text_input = nullptr;
}

void focus_detail_comment(lv_event_t*) {
    search_focused = true;
    active_text_input = detail_comment_input;
    active_text_target = TextInputTarget::DetailComment;
}

void unfocus_detail_comment(lv_event_t*) {
    if (active_text_input == detail_comment_input) active_text_input = nullptr;
    search_focused = false;
}

void focus_inbox_message(lv_event_t*) {
    search_focused = true;
    active_text_input = inbox_message_input;
    active_text_target = TextInputTarget::InboxMessage;
}

void focus_http_proxy(lv_event_t*) {
    search_focused = true;
    active_text_input = proxy_input;
    active_text_target = TextInputTarget::HttpProxy;
}

void unfocus_http_proxy(lv_event_t*) {
    if (active_text_input == proxy_input) active_text_input = nullptr;
    search_focused = false;
}

void save_http_proxy(lv_event_t*) {
    if (!proxy_input) return;
    setting_http_proxy = lv_textarea_get_text(proxy_input);
    setting_http_proxy.erase(std::remove_if(setting_http_proxy.begin(), setting_http_proxy.end(),
                                             [](char value) { return value == '"' || value == '\\' || value == '\r' || value == '\n'; }),
                             setting_http_proxy.end());
    lv_textarea_set_text(proxy_input, setting_http_proxy.c_str());
    platform_set_http_proxy(setting_http_proxy, setting_http_proxy_enabled);
    save_lvgl_config();
    if (status_label) lv_label_set_text(status_label, setting_http_proxy.empty() ? "Proxy address cleared" : "Proxy address saved");
}

void unfocus_inbox_message(lv_event_t*) {
    if (active_text_input == inbox_message_input) active_text_input = nullptr;
    search_focused = false;
}

void open_original_page(lv_event_t* event) {
    const size_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)) - 1;
    if (index >= displayed_videos.size()) return;
    const std::string url = std::string("https://www.bilibili.com/video/") + displayed_videos[index].bvid;
    platform_open_external_url(url);
}

void return_to_recommend(lv_event_t*) {
    ++navigation_epoch;
    if (player_page_visible || player_playing) leave_player_page();
    if (detail_return_to_season) {
        detail_return_to_season = false;
        show_season_detail_page();
        return;
    }
    if (detail_return_to_dynamic) {
        detail_return_to_dynamic = false;
        show_main_section(MainSection::Dynamic);
        return;
    }
    main_section = MainSection::Home;
    update_shell_for_section(main_section);
    if (nav_rail) lv_obj_clear_flag(nav_rail, LV_OBJ_FLAG_HIDDEN);
    // Returning from detail must restore the same snapshot the selected index
    // belongs to. A network refresh can replace pending_videos meanwhile.
    if (feed_mode == FeedMode::Search) show_search_results_page(displayed_videos, {});
    else show_recommend_page(displayed_videos, {});
}

void show_video_detail(size_t video_index) {
    if (video_index >= displayed_videos.size()) return;
    const Video& video = displayed_videos[video_index];
    active_video_index = video_index;
    player_page_visible = false;
    detail_comment_input = nullptr;
    active_text_input = nullptr;
    if (active_part_index >= video.parts.size()) active_part_index = 0;
    detail_visible = true;
    season_page_visible = false;
    if (board_preview) {
        if (nav_rail) lv_obj_clear_flag(nav_rail, LV_OBJ_FLAG_HIDDEN);
        if (status_label) lv_obj_clear_flag(status_label, LV_OBJ_FLAG_HIDDEN);
        for (lv_obj_t* control : home_header_controls) {
            if (control) lv_obj_add_flag(control, LV_OBJ_FLAG_HIDDEN);
        }
        if (top_title) lv_label_set_text(top_title, "Video");
        if (content) lv_obj_del(content);
        content = lv_obj_create(lv_scr_act());
        lv_obj_set_size(content, 720, 382);
        lv_obj_set_pos(content, 72, 100);
        lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(content, 0, 0);
        lv_obj_set_style_pad_all(content, 0, 0);
        lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
        lv_label_set_text(status_label, "Video detail / 800x480 compact preview");
        lv_obj_t* preview = lv_obj_create(content);
        lv_obj_set_size(preview, 440, 248);
        lv_obj_set_style_bg_color(preview, ui_color(0x05070b), 0);
        lv_obj_set_style_border_width(preview, 0, 0);
        lock_static_canvas(preview);
        if (video_index < thumbnail_descriptors.size() && thumbnail_descriptors[video_index].data) {
            lv_obj_t* image = lv_img_create(preview);
            lv_img_set_src(image, &thumbnail_descriptors[video_index]);
            lv_img_set_zoom(image, (420 * LV_IMG_ZOOM_NONE) / 186);
            lv_obj_center(image);
        }
        lv_obj_t* info = lv_obj_create(content);
        lv_obj_set_size(info, 264, 248);
        lv_obj_set_pos(info, 456, 0);
        lv_obj_set_style_bg_color(info, ui_color(0x1d2330), 0);
        lv_obj_set_style_radius(info, 4, 0);
        lv_obj_set_style_border_width(info, 0, 0);
        lock_static_canvas(info);
        lv_obj_t* title = lv_label_create(info);
        lv_label_set_text(title, video.title.c_str());
        lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
        lv_obj_set_size(title, 232, 110);
        lv_obj_set_style_text_font(title, ui_font(), 0);
        lv_obj_set_style_text_color(title, ui_color(0xf3f5f8), 0);
        lv_obj_set_pos(title, 14, 14);
        const std::string meta = "UP: " + video.owner + "\n" + video.view + " views\n" +
                                 (video.duration.empty() ? std::string{} : format_duration(video.duration) + "  ") +
                                 "CID: " + video.cid;
        set_label(info, meta.c_str(), 13, ui_color(0xaeb8c6), LV_ALIGN_BOTTOM_LEFT, 14, -14);
        lv_obj_t* back = lv_btn_create(content);
        lv_obj_set_size(back, 110, 40);
        lv_obj_set_pos(back, 0, 270);
        lv_obj_set_style_bg_color(back, ui_color(0x374152), 0);
        lv_obj_set_style_radius(back, 4, 0);
        lv_obj_set_style_border_width(back, 0, 0);
        lv_obj_add_event_cb(back, return_to_recommend, LV_EVENT_CLICKED, nullptr);
        set_label(back, "Back", 15, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        lv_obj_t* play = lv_btn_create(content);
        lv_obj_set_size(play, 110, 40);
        lv_obj_set_pos(play, 124, 270);
        lv_obj_set_style_bg_color(play, ui_color(0x2877a9), 0);
        lv_obj_set_style_radius(play, 4, 0);
        lv_obj_set_style_border_width(play, 0, 0);
        lv_obj_add_event_cb(play, [](lv_event_t* event) {
            show_player_page(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)) - 1);
        }, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<uintptr_t>(video_index + 1)));
        set_label(play, "Play", 15, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        if (video.parts.size() > 1) {
            lv_obj_t* next_part = lv_btn_create(content);
            lv_obj_set_size(next_part, 110, 40);
            lv_obj_set_pos(next_part, 248, 270);
            lv_obj_set_style_bg_color(next_part, ui_color(0x374152), 0);
            lv_obj_set_style_radius(next_part, 4, 0);
            lv_obj_set_style_border_width(next_part, 0, 0);
            lv_obj_add_event_cb(next_part, select_next_video_part, LV_EVENT_CLICKED, nullptr);
            const std::string label = "Part " + std::to_string(active_part_index + 1) + "/" +
                                      std::to_string(video.parts.size());
            set_label(next_part, label.c_str(), 13, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        }
        return;
    }
    for (lv_obj_t* control : home_header_controls) {
        if (control) lv_obj_add_flag(control, LV_OBJ_FLAG_HIDDEN);
    }
    if (top_title) lv_label_set_text(top_title, "Video");
    if (status_label) lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
    for (lv_obj_t* nav : main_nav) {
        if (nav) lv_obj_set_style_bg_color(nav, ui_color(0x2b3444), 0);
    }
    if (content) lv_obj_del(content);
    content = lv_obj_create(lv_scr_act());
    lv_obj_set_size(content, 1120, 510);
    // Center the fixed-height detail composition within the desktop browsing
    // area instead of stranding all unused vertical space below it.
    lv_obj_set_pos(content, 130, 150);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* player = lv_obj_create(content);
    lv_obj_set_size(player, 676, 397);
    lv_obj_set_pos(player, 0, 0);
    lv_obj_set_style_bg_color(player, ui_color(0x05070b), 0);
    lv_obj_set_style_border_width(player, 0, 0);
    lv_obj_set_style_radius(player, 6, 0);
    lock_static_canvas(player);
    if (video_index < thumbnail_descriptors.size() && thumbnail_descriptors[video_index].data) {
        lv_obj_t* cover = lv_img_create(player);
        lv_img_set_src(cover, &thumbnail_descriptors[video_index]);
        // LVGL images do not scale merely because their object box is resized.
        // The card cover is 186x104; 3x gives the intended 558x312 player poster.
        lv_img_set_zoom(cover, 3 * LV_IMG_ZOOM_NONE);
        lv_obj_center(cover);
        lv_obj_clear_flag(cover, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_t* shade = lv_obj_create(player);
    lv_obj_set_size(shade, 558, 312);
    lv_obj_center(shade);
    lv_obj_set_style_bg_color(shade, ui_color(0x000000), 0);
    lv_obj_set_style_bg_opa(shade, LV_OPA_30, 0);
    lv_obj_set_style_border_width(shade, 0, 0);
    lock_static_canvas(shade);
    lv_obj_t* play = lv_btn_create(player);
    lv_obj_set_size(play, 68, 68);
    lv_obj_center(play);
    lv_obj_set_style_bg_color(play, ui_color(0x2994d6), 0);
    lv_obj_set_style_radius(play, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(play, 0, 0);
    lv_obj_add_event_cb(play, [](lv_event_t* event) {
        show_player_page(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)) - 1);
    }, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<uintptr_t>(video_index + 1)));
    set_label(play, LV_SYMBOL_PLAY, 28, ui_color(0xffffff), LV_ALIGN_CENTER, 2, 0);

    lv_obj_t* info = lv_obj_create(content);
    lv_obj_set_size(info, 420, 540);
    lv_obj_set_pos(info, 700, 0);
    lv_obj_set_style_bg_color(info, ui_color(0x202735), 0);
    lv_obj_set_style_radius(info, 8, 0);
    lv_obj_set_style_border_width(info, 0, 0);
    lv_obj_set_style_pad_all(info, 24, 0);
    lock_static_canvas(info);

    lv_obj_t* title = lv_label_create(info);
    lv_label_set_text(title, video.title.c_str());
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(title, 370, 56);
    lv_obj_set_style_text_font(title, ui_font(), 0);
    lv_obj_set_style_text_color(title, ui_color(0xf3f5f8), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    const std::string metadata = "UP: " + (video.owner.empty() ? std::string("Bilibili") : video.owner) +
                                 "\n" + (video.view.empty() ? std::string("Views unavailable") : video.view + " views") +
                                 (video.duration.empty() ? std::string{} : "  |  " + format_duration(video.duration));
    lv_obj_t* meta = lv_label_create(info);
    lv_label_set_text(meta, metadata.c_str());
    lv_label_set_long_mode(meta, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(meta, 370, 46);
    lv_obj_set_style_text_font(meta, ui_font(), 0);
    lv_obj_set_style_text_color(meta, ui_color(0xaeb8c6), 0);
    lv_obj_align(meta, LV_ALIGN_TOP_LEFT, 0, 72);
    const char* stat_titles[] = {"Views", "Comments", "Length"};
    const std::string stat_values[] = {
        video.view.empty() ? "--" : video.view,
        video.comment_count.empty() ? "--" : video.comment_count,
        video.duration.empty() ? "--" : format_duration(video.duration),
    };
    for (int index = 0; index < 3; ++index) {
        lv_obj_t* stat = lv_obj_create(info);
        lv_obj_set_size(stat, 116, 62);
        lv_obj_set_pos(stat, index * 126, 130);
        lv_obj_set_style_bg_color(stat, ui_color(0x283348), 0);
        lv_obj_set_style_radius(stat, 4, 0);
        lv_obj_set_style_border_width(stat, 0, 0);
        lock_static_canvas(stat);
        set_label(stat, stat_titles[index], 12, ui_color(0x9dacbd), LV_ALIGN_TOP_LEFT, 10, 8);
        set_label(stat, stat_values[index].c_str(), 15, ui_color(0xf3f5f8), LV_ALIGN_BOTTOM_LEFT, 10, -9);
    }
    // Keep the compact detail surface factual. Long upstream descriptions are
    // available on the original page; rendering them here competes with the
    // fixed comment/action area and causes text to overlap controls.

    lv_obj_t* original = lv_btn_create(info);
    lv_obj_set_size(original, 370, 50);
    lv_obj_set_pos(original, 24, 370);
    lv_obj_set_style_bg_color(original, ui_color(0x2877a9), 0);
    lv_obj_set_style_radius(original, 6, 0);
    lv_obj_add_event_cb(original, open_original_page, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(video_index + 1)));
    set_label(original, "Open original Bilibili page", 16, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* like = lv_btn_create(info);
    lv_obj_set_size(like, 116, 42);
    lv_obj_set_pos(like, 24, 308);
    lv_obj_set_style_bg_color(like, ui_color(0x374152), 0);
    lv_obj_set_style_radius(like, 6, 0);
    lv_obj_set_style_border_width(like, 0, 0);
    lv_obj_add_event_cb(like, like_active_video, LV_EVENT_CLICKED, nullptr);
    set_label(like, "Like", 14, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* favorite = lv_btn_create(info);
    lv_obj_set_size(favorite, 116, 42);
    lv_obj_set_pos(favorite, 151, 308);
    lv_obj_set_style_bg_color(favorite, ui_color(0x374152), 0);
    lv_obj_set_style_radius(favorite, 6, 0);
    lv_obj_set_style_border_width(favorite, 0, 0);
    lv_obj_add_event_cb(favorite, favorite_active_video, LV_EVENT_CLICKED, nullptr);
    set_label(favorite, "Favorite", 13, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* submit_comment = lv_btn_create(info);
    lv_obj_set_size(submit_comment, 116, 42);
    lv_obj_set_pos(submit_comment, 278, 308);
    lv_obj_set_style_bg_color(submit_comment, ui_color(0x374152), 0);
    lv_obj_set_style_radius(submit_comment, 6, 0);
    lv_obj_set_style_border_width(submit_comment, 0, 0);
    lv_obj_add_event_cb(submit_comment, submit_video_comment, LV_EVENT_CLICKED, nullptr);
    set_label(submit_comment, "Comment", 14, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);

    detail_comment_input = lv_textarea_create(info);
    lv_obj_set_size(detail_comment_input, 370, 42);
    lv_obj_set_pos(detail_comment_input, 24, 250);
    lv_textarea_set_one_line(detail_comment_input, true);
    lv_textarea_set_placeholder_text(detail_comment_input, "Write a comment");
    style_dark_textarea(detail_comment_input);
    lv_obj_add_event_cb(detail_comment_input, focus_detail_comment, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(detail_comment_input, unfocus_detail_comment, LV_EVENT_DEFOCUSED, nullptr);

    if (video.parts.size() > 1) {
        lv_obj_t* next_part = lv_btn_create(info);
        lv_obj_set_size(next_part, 370, 42);
        lv_obj_align(next_part, LV_ALIGN_BOTTOM_MID, 0, -208);
        lv_obj_set_style_bg_color(next_part, ui_color(0x374152), 0);
        lv_obj_set_style_radius(next_part, 6, 0);
        lv_obj_set_style_border_width(next_part, 0, 0);
        lv_obj_add_event_cb(next_part, select_next_video_part, LV_EVENT_CLICKED, nullptr);
        const std::string label = "Part " + std::to_string(active_part_index + 1) + "/" +
                                  std::to_string(video.parts.size()) + ": " + video.parts[active_part_index].title;
        set_label(next_part, label.c_str(), 14, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    }

    lv_obj_t* back = lv_btn_create(info);
    lv_obj_set_size(back, 370, 42);
    lv_obj_set_pos(back, 24, 438);
    lv_obj_set_style_bg_color(back, ui_color(0x374152), 0);
    lv_obj_set_style_radius(back, 6, 0);
    lv_obj_add_event_cb(back, return_to_recommend, LV_EVENT_CLICKED, nullptr);
    set_label(back, detail_return_to_season ? "Back to Season" :
                    detail_return_to_dynamic ? "Back to Dynamic" : "Back to Recommend", 16,
              ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
}

void clear_player_controls(lv_event_t*) {
    player_seek_label = nullptr;
    player_volume_label = nullptr;
    danmaku_label = nullptr;
    danmaku_tracks.fill(nullptr);
}

void leave_player_page() {
    platform_player_stop();
    player_playing = false;
    player_page_visible = false;
    player_elapsed_offset_ms = 0;
    player_session_bvid.clear();
    player_session_part_index = 0;
    clear_player_controls(nullptr);
    if (fullscreen_active) {
        platform_display_set_fullscreen(false);
        fullscreen_active = false;
    }
}

void return_to_detail(lv_event_t*) {
    leave_player_page();
    show_video_detail(active_video_index);
}

void toggle_player_playback(lv_event_t* event) {
    if (player_playing) {
        player_elapsed_offset_ms += (platform_ticks() - player_started_tick) *
                                    static_cast<uint32_t>(setting_playback_speed) / 100U;
    }
    std::string error;
    const bool changed = player_playing ? platform_player_pause(error) : platform_player_start(error);
    if (!changed) {
        if (player_playing) player_elapsed_offset_ms -= (platform_ticks() - player_started_tick) *
                                                         static_cast<uint32_t>(setting_playback_speed) / 100U;
        player_backend_error = error;
        if (status_label) lv_label_set_text(status_label, player_backend_error.c_str());
        return;
    }
    player_playing = !player_playing;
    if (player_playing) {
        player_started_tick = platform_ticks();
        request_danmaku(active_video_index);
    }
    if (player_playing && !player_history_reported && active_video_index < displayed_videos.size()) {
        report_playback_history(displayed_videos[active_video_index]);
        player_history_reported = true;
    }
    lv_obj_t* button = static_cast<lv_obj_t*>(lv_event_get_target(event));
    lv_obj_t* label = lv_obj_get_child(button, 0);
    if (label) lv_label_set_text(label, player_playing ? "Pause" : "Play");
}

void seek_player(lv_event_t* event) {
    const int seconds = lv_slider_get_value(static_cast<lv_obj_t*>(lv_event_get_target(event)));
    std::string error;
    if (!platform_player_seek(static_cast<uint32_t>(seconds) * 1000, error)) {
        player_backend_error = error;
        if (status_label) lv_label_set_text(status_label, player_backend_error.c_str());
        return;
    }
    player_elapsed_offset_ms = static_cast<uint32_t>(seconds) * 1000U;
    player_started_tick = platform_ticks();
    if (player_seek_label) {
        const std::string label = std::to_string(seconds / 60) + ":" +
                                  (seconds % 60 < 10 ? "0" : "") + std::to_string(seconds % 60);
        lv_label_set_text(player_seek_label, label.c_str());
    }
    if (active_video_index < displayed_videos.size()) {
        report_playback_history(displayed_videos[active_video_index], static_cast<uint32_t>(seconds));
    }
}

void set_player_volume(lv_event_t* event) {
    const int percent = lv_slider_get_value(static_cast<lv_obj_t*>(lv_event_get_target(event)));
    setting_volume = percent;
    save_lvgl_config();
    std::string error;
    if (!platform_player_set_volume(static_cast<uint8_t>(percent), error)) {
        player_backend_error = error;
        if (status_label) lv_label_set_text(status_label, player_backend_error.c_str());
        return;
    }
    if (player_volume_label) {
        const std::string label = "Vol " + std::to_string(percent);
        lv_label_set_text(player_volume_label, label.c_str());
    }
}

void show_player_page(size_t video_index) {
    if (video_index >= displayed_videos.size()) return;
    const Video& video = displayed_videos[video_index];
    const bool new_player_session = player_session_bvid != video.bvid ||
                                    player_session_part_index != active_part_index;
    if (new_player_session) {
        platform_player_stop();
        player_playing = false;
        player_history_reported = false;
        player_elapsed_offset_ms = 0;
        player_session_bvid = video.bvid;
        player_session_part_index = active_part_index;
    }
    active_video_index = video_index;
    player_page_visible = true;
    if (!board_preview && setting_autofullscreen && !fullscreen_active) {
        platform_display_set_fullscreen(true);
        fullscreen_active = true;
    }
    player_seek_label = nullptr;
    player_volume_label = nullptr;
    danmaku_label = nullptr;
    danmaku_tracks.fill(nullptr);
    player_backend_error.clear();
    PlatformPlayerSpec player_spec;
    player_spec.video_id = video.bvid;
    player_spec.stream_uri = h264_test_uri.empty() ? video.play_url : h264_test_uri;
    player_spec.audio_stream_uri = h264_test_uri.empty() ? video.play_audio_url : std::string{};
    player_spec.input = h264_test_uri.empty() ? PlatformVideoInput::ContainerStream : PlatformVideoInput::H264AnnexB;
    player_spec.decode_width = static_cast<uint16_t>(board_preview ? 800 : kWindowWidth);
    player_spec.decode_height = static_cast<uint16_t>(board_preview ? 480 : kWindowHeight);
    player_spec.rotation_degrees = board_preview ? 90 : 0;
    player_spec.output_width = static_cast<uint16_t>(board_preview ? 480 : kWindowWidth);
    player_spec.output_height = static_cast<uint16_t>(board_preview ? 800 : kWindowHeight);
    player_spec.video_quality = setting_video_quality;
    player_spec.video_format = setting_video_format;
    player_spec.video_codec = setting_video_codec;
    player_spec.audio_quality = setting_audio_quality;
    player_spec.memory_cache_mib = setting_memory_cache;
    player_spec.playback_speed_percent = setting_playback_speed;
    player_spec.volume_percent = setting_volume;
    player_spec.aspect = setting_aspect;
    player_spec.low_quality = setting_low_quality;
    player_spec.tv_osd = setting_tv_osd;
    player_spec.separate_video_plane = true;
    player_spec.native_size_only = board_preview;
    std::string prepare_error;
    if (!platform_player_prepare(player_spec, prepare_error)) player_backend_error = prepare_error;
    if (player_backend_error.empty() && !player_spec.stream_uri.empty()) {
        std::string start_error;
        if (platform_player_start(start_error)) {
            const bool starting_from_stop = !player_playing;
            player_playing = true;
            if (starting_from_stop) player_started_tick = platform_ticks();
            if (!player_history_reported) {
                report_playback_history(video);
                player_history_reported = true;
            }
            if (new_player_session) request_danmaku(video_index);
        }
        else player_backend_error = start_error;
    }
    if (board_preview) {
        for (lv_obj_t* control : home_header_controls) {
            if (control) lv_obj_add_flag(control, LV_OBJ_FLAG_HIDDEN);
        }
        if (top_title) lv_label_set_text(top_title, "Player");
        if (nav_rail) lv_obj_add_flag(nav_rail, LV_OBJ_FLAG_HIDDEN);
        if (content) lv_obj_del(content);
        content = lv_obj_create(lv_scr_act());
        lv_obj_add_event_cb(content, clear_player_controls, LV_EVENT_DELETE, nullptr);
        lv_obj_set_size(content, kBoardWidth, kBoardHeight);
        lv_obj_set_pos(content, 0, 0);
        lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(content, 0, 0);
        lv_obj_set_style_pad_all(content, 0, 0);
        lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
        if (status_label) lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_t* surface = lv_obj_create(content);
        // Windows draws a preview image here only. Buildroot gives this exact
        // rectangle to the independent Cedar/DE video plane, without scaling.
        lv_obj_set_size(surface, kBoardWidth, kBoardHeight);
        lv_obj_set_pos(surface, 0, 0);
        lv_obj_set_style_bg_color(surface, ui_color(0x05070b), 0);
        lv_obj_set_style_border_width(surface, 0, 0);
        lv_obj_clear_flag(surface, LV_OBJ_FLAG_SCROLLABLE);
        if (video_index < thumbnail_descriptors.size() && thumbnail_descriptors[video_index].data) {
            lv_obj_t* image = lv_img_create(surface);
            lv_img_set_src(image, &thumbnail_descriptors[video_index]);
            lv_img_set_zoom(image, (800 * LV_IMG_ZOOM_NONE) / 186);
            lv_obj_center(image);
        }
        lv_obj_t* shade = lv_obj_create(surface);
        lv_obj_set_size(shade, kBoardWidth, kBoardHeight);
        lv_obj_set_style_bg_color(shade, ui_color(0x000000), 0);
        lv_obj_set_style_bg_opa(shade, LV_OPA_20, 0);
        lv_obj_set_style_border_width(shade, 0, 0);
        lv_obj_clear_flag(shade, LV_OBJ_FLAG_CLICKABLE);
        danmaku_tracks.fill(nullptr);
        const size_t visible_tracks = setting_danmaku_area <= 25 ? 1 : setting_danmaku_area <= 50 ? 2 : 3;
        for (size_t index = 0; index < danmaku_tracks.size(); ++index) {
            lv_obj_t* track = lv_label_create(surface);
            lv_label_set_text(track, "");
            lv_label_set_long_mode(track, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(track, 680);
            lv_obj_set_style_text_font(track, font_for_text("danmaku", 13), 0);
            lv_obj_set_style_text_color(track, lv_color_hex(0xffffff), 0);
            lv_obj_set_style_text_opa(track, static_cast<lv_opa_t>(setting_danmaku_alpha * 255 / 100), 0);
            lv_obj_align(track, LV_ALIGN_TOP_MID, 0, 58 + static_cast<int>(index) * 42);
            if (index >= visible_tracks) lv_obj_add_flag(track, LV_OBJ_FLAG_HIDDEN);
            danmaku_tracks[index] = track;
        }
        danmaku_label = danmaku_tracks[0];
        lv_obj_t* controls = lv_obj_create(content);
        lv_obj_set_size(controls, kBoardWidth, 86);
        lv_obj_set_pos(controls, 0, 394);
        lv_obj_set_style_bg_color(controls, ui_color(0x101722), 0);
        lv_obj_set_style_bg_opa(controls, LV_OPA_80, 0);
        lv_obj_set_style_border_width(controls, 0, 0);
        lv_obj_set_style_pad_all(controls, 0, 0);
        lock_static_canvas(controls);
        lv_obj_t* back = lv_btn_create(controls);
        lv_obj_set_size(back, 90, 40);
        lv_obj_set_pos(back, 16, 22);
        lv_obj_set_style_bg_color(back, ui_color(0x374152), 0);
        lv_obj_set_style_radius(back, 4, 0);
        lv_obj_set_style_border_width(back, 0, 0);
        lv_obj_add_event_cb(back, return_to_detail, LV_EVENT_CLICKED, nullptr);
        set_label(back, "Back", 15, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        lv_obj_t* play = lv_btn_create(controls);
        lv_obj_set_size(play, 90, 40);
        lv_obj_set_pos(play, 116, 22);
        lv_obj_set_style_bg_color(play, ui_color(0x2877a9), 0);
        lv_obj_set_style_radius(play, 4, 0);
        lv_obj_set_style_border_width(play, 0, 0);
        lv_obj_add_event_cb(play, toggle_player_playback, LV_EVENT_CLICKED, nullptr);
        set_label(play, player_playing ? "Pause" : "Play", 15, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        lv_obj_t* seek = lv_slider_create(controls);
        lv_obj_set_size(seek, 260, 12);
        lv_obj_set_pos(seek, 226, 38);
        lv_slider_set_range(seek, 0, 600);
        lv_obj_add_event_cb(seek, seek_player, LV_EVENT_VALUE_CHANGED, nullptr);
        player_seek_label = lv_label_create(controls);
        lv_label_set_text(player_seek_label, "0:00");
        lv_obj_set_style_text_font(player_seek_label, font_for_text("0:00", 12), 0);
        lv_obj_set_style_text_color(player_seek_label, ui_color(0xaeb8c6), 0);
        lv_obj_set_pos(player_seek_label, 496, 30);
        lv_obj_t* volume = lv_slider_create(controls);
        lv_obj_set_size(volume, 112, 12);
        lv_obj_set_pos(volume, 584, 38);
        lv_slider_set_range(volume, 0, 100);
        lv_slider_set_value(volume, setting_volume, LV_ANIM_OFF);
        lv_obj_add_event_cb(volume, set_player_volume, LV_EVENT_VALUE_CHANGED, nullptr);
        player_volume_label = lv_label_create(controls);
        lv_label_set_text(player_volume_label, (std::string("Vol ") + std::to_string(setting_volume)).c_str());
        lv_obj_set_style_text_font(player_volume_label, font_for_text("Vol 70", 12), 0);
        lv_obj_set_style_text_color(player_volume_label, ui_color(0xaeb8c6), 0);
        lv_obj_set_pos(player_volume_label, 704, 30);
        lv_obj_t* speed = lv_btn_create(content);
        lv_obj_set_size(speed, 84, 28);
        lv_obj_set_pos(speed, 700, 14);
        lv_obj_add_event_cb(speed, cycle_playback_speed, LV_EVENT_CLICKED, nullptr);
        const std::string speed_text = "Speed " + std::to_string(setting_playback_speed / 100) + "." +
                                       std::to_string((setting_playback_speed % 100) / 10) + "x";
        set_label(speed, speed_text.c_str(), 12, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        // The 800x480 player is a full-screen OSD.  A full upstream title
        // competes with the seek/volume controls and can overflow the video
        // plane, so leave this layer to controls only.
        return;
    }
    for (lv_obj_t* control : home_header_controls) {
        if (control) lv_obj_add_flag(control, LV_OBJ_FLAG_HIDDEN);
    }
    if (top_title) lv_label_set_text(top_title, "Player");
    if (status_label) lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
    for (lv_obj_t* nav : main_nav) {
        if (nav) lv_obj_set_style_bg_color(nav, ui_color(0x2b3444), 0);
    }
    if (content) lv_obj_del(content);
    detail_visible = true;
    content = lv_obj_create(lv_scr_act());
    lv_obj_add_event_cb(content, clear_player_controls, LV_EVENT_DELETE, nullptr);
    lv_obj_set_size(content, 1120, 590);
    lv_obj_set_pos(content, 130, 98);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* left = lv_obj_create(content);
    lv_obj_set_size(left, 705, 570);
    lv_obj_set_pos(left, 0, 0);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 0, 0);
    lock_static_canvas(left);
    lv_obj_t* surface = lv_obj_create(left);
    lv_obj_set_size(surface, 705, 420);
    lv_obj_set_style_bg_color(surface, ui_color(0x05070b), 0);
    lv_obj_set_style_border_width(surface, 0, 0);
    lv_obj_set_style_radius(surface, 6, 0);
    if (video_index < thumbnail_descriptors.size() && thumbnail_descriptors[video_index].data) {
        lv_obj_t* cover = lv_img_create(surface);
        lv_img_set_src(cover, &thumbnail_descriptors[video_index]);
        lv_img_set_zoom(cover, 4 * LV_IMG_ZOOM_NONE);
        lv_obj_center(cover);
        lv_obj_clear_flag(cover, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_t* shade = lv_obj_create(surface);
    lv_obj_set_size(shade, 705, 420);
    lv_obj_set_style_bg_color(shade, ui_color(0x000000), 0);
    lv_obj_set_style_bg_opa(shade, LV_OPA_50, 0);
    lv_obj_set_style_border_width(shade, 0, 0);
    lv_obj_clear_flag(shade, LV_OBJ_FLAG_CLICKABLE);
    set_label(surface, player_playing ? "Windows decoder is active" :
              (video.play_url.empty() ? "H264 Annex-B decoder is not attached" :
               "Bilibili container stream is ready"),
              18, ui_color(0xffffff), LV_ALIGN_CENTER, 0, -16);
    set_label(surface, setting_tv_osd ? "TV OSD mode enabled; controls use compact layout" :
              (player_playing ? "Windows decoder output is in a separate window" :
              "Timed danmaku waits for playback"), 14,
              ui_color(0xc4ceda), LV_ALIGN_CENTER, 0, 16);
    danmaku_tracks.fill(nullptr);
    const size_t visible_tracks = setting_danmaku_area <= 25 ? 1 : setting_danmaku_area <= 50 ? 2 : 3;
    for (size_t index = 0; index < danmaku_tracks.size(); ++index) {
        lv_obj_t* track = lv_label_create(surface);
        lv_label_set_text(track, "");
        lv_label_set_long_mode(track, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(track, 690);
        lv_obj_set_style_text_font(track, font_for_text("danmaku", 14), 0);
        lv_obj_set_style_text_color(track, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_opa(track, static_cast<lv_opa_t>(setting_danmaku_alpha * 255 / 100), 0);
        lv_obj_align(track, LV_ALIGN_TOP_MID, 0, 92 + static_cast<int>(index) * 50);
        if (index >= visible_tracks) lv_obj_add_flag(track, LV_OBJ_FLAG_HIDDEN);
        danmaku_tracks[index] = track;
    }
    danmaku_label = danmaku_tracks[0];
    lv_obj_t* back = lv_btn_create(surface);
    lv_obj_set_size(back, 100, 38);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 14, 14);
    lv_obj_set_style_bg_color(back, ui_color(0x374152), 0);
    lv_obj_set_style_radius(back, 5, 0);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_add_event_cb(back, return_to_detail, LV_EVENT_CLICKED, nullptr);
    set_label(back, "Back", 15, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    lv_obj_t* play = lv_btn_create(surface);
    lv_obj_set_size(play, 100, 38);
    lv_obj_align(play, LV_ALIGN_TOP_LEFT, 124, 14);
    lv_obj_set_style_bg_color(play, ui_color(0x2877a9), 0);
    lv_obj_set_style_radius(play, 5, 0);
    lv_obj_set_style_border_width(play, 0, 0);
    lv_obj_add_event_cb(play, toggle_player_playback, LV_EVENT_CLICKED, nullptr);
    set_label(play, player_playing ? "Pause" : "Play", 15, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    lv_obj_t* seek = lv_slider_create(left);
    lv_obj_set_size(seek, 520, 12);
    lv_obj_set_pos(seek, 10, 485);
    lv_slider_set_range(seek, 0, 600);
    lv_obj_add_event_cb(seek, seek_player, LV_EVENT_VALUE_CHANGED, nullptr);
    player_seek_label = lv_label_create(left);
    lv_label_set_text(player_seek_label, "0:00");
    lv_obj_set_style_text_font(player_seek_label, font_for_text("0:00", 14), 0);
    lv_obj_set_style_text_color(player_seek_label, ui_color(0xaeb8c6), 0);
    lv_obj_set_pos(player_seek_label, 540, 475);
    lv_obj_t* volume = lv_slider_create(left);
    lv_obj_set_size(volume, 110, 12);
    lv_obj_set_pos(volume, 590, 485);
    lv_slider_set_range(volume, 0, 100);
    lv_slider_set_value(volume, setting_volume, LV_ANIM_OFF);
    lv_obj_add_event_cb(volume, set_player_volume, LV_EVENT_VALUE_CHANGED, nullptr);
    player_volume_label = lv_label_create(left);
    lv_label_set_text(player_volume_label, (std::string("Vol ") + std::to_string(setting_volume)).c_str());
    lv_obj_set_style_text_font(player_volume_label, font_for_text("Vol 70", 14), 0);
    lv_obj_set_style_text_color(player_volume_label, ui_color(0xaeb8c6), 0);
    lv_obj_set_pos(player_volume_label, 590, 505);
    lv_obj_t* speed = lv_btn_create(left);
    lv_obj_set_size(speed, 96, 34);
    lv_obj_set_pos(speed, 495, 500);
    lv_obj_add_event_cb(speed, cycle_playback_speed, LV_EVENT_CLICKED, nullptr);
    const std::string speed_text = "Speed " + std::to_string(setting_playback_speed / 100) + "." +
                                   std::to_string((setting_playback_speed % 100) / 10) + "x";
    set_label(speed, speed_text.c_str(), 13, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    lv_obj_t* title = lv_label_create(left);
    lv_label_set_text(title, video.title.c_str());
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title, 685);
    lv_obj_set_style_text_font(title, ui_font(), 0);
    lv_obj_set_style_text_color(title, ui_color(0xf3f5f8), 0);
    lv_obj_set_pos(title, 10, 438);
    const std::string info = "UP: " + video.owner + "    " + video.view + " views    " + video.bvid;
    set_label(left, info.c_str(), 15, ui_color(0xaeb8c6), LV_ALIGN_BOTTOM_LEFT, 10, -8);

    lv_obj_t* right = lv_obj_create(content);
    lv_obj_set_size(right, 395, 570);
    lv_obj_set_pos(right, 725, 0);
    lv_obj_set_style_bg_color(right, ui_color(0x1d2330), 0);
    lv_obj_set_style_radius(right, 6, 0);
    lv_obj_set_style_border_width(right, 0, 0);
    lock_static_canvas(right);
    const char* panels[] = {"Comments", "Parts", "Related"};
    for (int i = 0; i < 3; ++i) {
        lv_obj_t* tab = lv_btn_create(right);
        lv_obj_set_size(tab, 112, 38);
        lv_obj_set_pos(tab, 12 + i * 122, 14);
        lv_obj_set_style_bg_color(tab, ui_color(static_cast<int>(player_side_panel) == i ? 0x2877a9 : 0x374152), 0);
        lv_obj_set_style_radius(tab, 5, 0);
        lv_obj_set_style_border_width(tab, 0, 0);
        lv_obj_add_event_cb(tab, select_player_side_panel, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
        set_label(tab, panels[i], 14, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    }
    // Keep the selector fixed.  Only the list below it is allowed to scroll,
    // so the panel still reads as a coherent player sidebar at any offset.
    lv_obj_t* panel_body = lv_obj_create(right);
    lv_obj_set_size(panel_body, 371, 488);
    lv_obj_set_pos(panel_body, 12, 68);
    lv_obj_set_style_bg_opa(panel_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(panel_body, 0, 0);
    lv_obj_set_style_pad_all(panel_body, 0, 0);
    lv_obj_set_scroll_dir(panel_body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(panel_body, LV_SCROLLBAR_MODE_AUTO);
    if (player_side_panel == PlayerSidePanel::Parts) {
        if (video.parts.empty()) {
            set_label(panel_body, "No parts were returned\nfor this video.",
                      15, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 18);
        } else {
            const size_t count = std::min<size_t>(video.parts.size(), 4);
            for (size_t index = 0; index < count; ++index) {
                const VideoPart& part = video.parts[index];
                lv_obj_t* item = lv_btn_create(panel_body);
                lv_obj_set_size(item, 355, 68);
                lv_obj_set_pos(item, 0, static_cast<int>(index) * 76);
                lv_obj_set_style_bg_color(item, ui_color(index == active_part_index ? 0x2877a9 : 0x202735), 0);
                lv_obj_set_style_radius(item, 4, 0);
                lv_obj_set_style_border_width(item, 0, 0);
                lv_obj_add_event_cb(item, select_player_video_part, LV_EVENT_CLICKED,
                                    reinterpret_cast<void*>(static_cast<uintptr_t>(index)));
                const std::string label = std::to_string(index + 1) + ". " + part.title;
                lv_obj_t* part_title = lv_label_create(item);
                lv_label_set_text(part_title, label.c_str());
                lv_label_set_long_mode(part_title, LV_LABEL_LONG_DOT);
                lv_obj_set_size(part_title, 274, 26);
                lv_obj_set_style_text_font(part_title, ui_font(), 0);
                lv_obj_set_style_text_color(part_title, ui_color(0xffffff), 0);
                lv_obj_align(part_title, LV_ALIGN_LEFT_MID, 12, 0);
                const std::string duration = format_duration(part.duration);
                set_label(item, duration.c_str(), 13, ui_color(0xc8d4df), LV_ALIGN_RIGHT_MID, -12, 0);
            }
        }
    } else if (player_side_panel == PlayerSidePanel::Related) {
        if (video.related.empty()) {
            set_label(panel_body, "No related videos were returned\nfor this video.",
                      15, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 18);
        } else {
            for (size_t index = 0; index < video.related.size(); ++index) {
                const RelatedVideo& related = video.related[index];
                lv_obj_t* item = lv_btn_create(panel_body);
                lv_obj_set_size(item, 355, 80);
                lv_obj_set_pos(item, 0, static_cast<int>(index) * 88);
                lv_obj_set_style_bg_color(item, ui_color(0x202735), 0);
                lv_obj_set_style_radius(item, 4, 0);
                lv_obj_set_style_border_width(item, 0, 0);
                lock_static_canvas(item);
                lv_obj_add_event_cb(item, open_related_video, LV_EVENT_CLICKED,
                                    reinterpret_cast<void*>(static_cast<uintptr_t>(index)));
                lv_obj_t* index_badge = lv_obj_create(item);
                lv_obj_set_size(index_badge, 28, 56);
                lv_obj_set_pos(index_badge, 12, 12);
                lv_obj_set_style_bg_color(index_badge, ui_color(0x2877a9), 0);
                lv_obj_set_style_radius(index_badge, 4, 0);
                lv_obj_set_style_border_width(index_badge, 0, 0);
                lock_static_canvas(index_badge);
                const std::string ordinal = index < 9 ? "0" + std::to_string(index + 1) : std::to_string(index + 1);
                set_label(index_badge, ordinal.c_str(), 13, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
                lv_obj_t* title = lv_label_create(item);
                lv_label_set_text(title, related.title.c_str());
                lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
                lv_obj_set_size(title, 286, 30);
                lv_obj_set_style_text_font(title, ui_font(), 0);
                lv_obj_set_style_text_color(title, ui_color(0xf3f5f8), 0);
                lv_obj_set_pos(title, 52, 12);
                const std::string meta = related.owner.empty() ? related.view + " views" :
                                         related.owner + (related.view.empty() ? "" : "  " + related.view + " views");
                lv_obj_t* metadata = lv_label_create(item);
                lv_label_set_text(metadata, meta.c_str());
                lv_label_set_long_mode(metadata, LV_LABEL_LONG_DOT);
                lv_obj_set_size(metadata, 286, 20);
                lv_obj_set_style_text_font(metadata, ui_font(), 0);
                lv_obj_set_style_text_color(metadata, ui_color(0x9dacbd), 0);
                lv_obj_set_pos(metadata, 52, 47);
            }
        }
    } else if (video.comments.empty()) {
        set_label(panel_body, "No public comments were returned\nfor this video.",
                  15, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 18);
    } else {
        for (size_t index = 0; index < video.comments.size(); ++index) {
            const VideoComment& comment = video.comments[index];
            lv_obj_t* item = lv_obj_create(panel_body);
            lv_obj_set_size(item, 355, 88);
            lv_obj_set_pos(item, 0, static_cast<int>(index) * 96);
            lv_obj_set_style_bg_color(item, ui_color(0x202735), 0);
            lv_obj_set_style_radius(item, 4, 0);
            lv_obj_set_style_border_width(item, 0, 0);
            lock_static_canvas(item);
            set_label(item, comment.author.c_str(), 14, ui_color(0x58b9ee), LV_ALIGN_TOP_LEFT, 10, 8);
            lv_obj_t* message = lv_label_create(item);
            lv_label_set_text(message, comment.message.c_str());
            // Comments are a scan list: reserve exactly two lines per item.
            // Long replies remain accessible through the original Bilibili
            // page without overflowing the next card.
            lv_label_set_long_mode(message, LV_LABEL_LONG_CLIP);
            lv_obj_set_size(message, 332, 44);
            lv_obj_set_style_text_font(message, ui_font(), 0);
            lv_obj_set_style_text_color(message, ui_color(0xf3f5f8), 0);
            lv_obj_set_pos(message, 10, 30);
        }
        lv_obj_t* more = lv_btn_create(panel_body);
        lv_obj_set_size(more, 355, 38);
        lv_obj_set_pos(more, 0, static_cast<int>(video.comments.size()) * 96);
        lv_obj_set_style_bg_color(more, ui_color(0x374152), 0);
        lv_obj_set_style_border_width(more, 0, 0);
        const bool exhausted = video.comments_exhausted || video.comments.size() >= kMaxVideoComments;
        if (exhausted) lv_obj_add_state(more, LV_STATE_DISABLED);
        else lv_obj_add_event_cb(more, load_more_video_comments, LV_EVENT_CLICKED, nullptr);
        set_label(more, video.comments.size() >= kMaxVideoComments ? "Comment limit reached" :
                       exhausted ? "No more comments" : "More comments", 13,
                  ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    }
}

void cycle_board_quality_profile(lv_event_t*) {
    static const int qn[] = {16, 32, 64};
    static const int audio[] = {30216, 30232, 30280};
    static const int cache[] = {32, 64, 128};
    size_t index = 0;
    for (size_t i = 0; i < 3; ++i) if (qn[i] == setting_video_quality) index = i;
    index = (index + 1) % 3;
    setting_video_quality = qn[index];
    setting_audio_quality = audio[index];
    setting_memory_cache = cache[index];
    save_lvgl_config();
    if (board_preview) show_main_section(MainSection::Settings);
}

bool json_video_title(const std::string& body, std::string& title) {
    const size_t data = body.find("\"data\":{");
    if (data == std::string::npos) return false;
    constexpr char title_key[] = "\"title\":\"";
    int depth = 1;
    bool in_string = false;
    bool escaped = false;
    for (size_t position = data + 8; position < body.size() && depth > 0; ++position) {
        if (!in_string && depth == 1 && body.compare(position, sizeof(title_key) - 1, title_key) == 0) {
            return json_string_at(body.substr(position, std::min(body.size() - position, size_t{2048})),
                                  "title", title);
        }
        const char character = body[position];
        if (in_string) {
            if (escaped) escaped = false;
            else if (character == 0x5c) escaped = true;
            else if (character == '"') in_string = false;
        } else if (character == '"') in_string = true;
        else if (character == '{' || character == '[') ++depth;
        else if (character == '}' || character == ']') --depth;
    }
    return false;
}

void request_video_detail(size_t video_index, size_t part_index) {
    if (video_index >= displayed_videos.size() || detail_fetch_in_flight.exchange(true)) return;
    const Video source = displayed_videos[video_index];
    const uint32_t request_epoch = navigation_epoch.load();
    if (status_label) lv_label_set_text(status_label, "Loading Bilibili video detail...");
    std::thread([source, video_index, part_index, request_epoch] {
        Video detail = source;
        std::string body;
        std::string error;
        if (fetch_feed("/x/web-interface/view?bvid=" + url_encode(source.bvid), body, error)) {
            json_number_at(body, "cid", detail.cid);
            json_number_at(body, "aid", detail.aid);
            json_number_at(body, "duration", detail.duration);
            json_string_at(body, "desc", detail.description);
            const size_t owner = body.find("\"owner\":{");
            if (owner != std::string::npos) json_string_at(body.substr(owner), "name", detail.owner);
            const size_t stat = body.find("\"stat\":{");
            if (stat != std::string::npos) {
                const std::string stats = body.substr(stat);
                json_number_at(stats, "reply", detail.comment_count);
                json_number_at(stats, "view", detail.view);
            }
            detail.parts = parse_video_parts(body);
            if (!detail.parts.empty()) {
                const size_t selected_part = std::min(part_index, detail.parts.size() - 1);
                detail.cid = detail.parts[selected_part].cid;
                detail.duration = detail.parts[selected_part].duration;
            }
            std::string title;
            if (json_video_title(body, title) && !title.empty()) detail.title = title;
            if (!detail.cid.empty()) {
                std::string play_body;
                std::string play_error;
                const int requested_qn = setting_low_quality ? 16 : setting_video_quality;
                const std::string play_path = "/x/player/wbi/playurl?bvid=" + url_encode(detail.bvid) +
                                              "&cid=" + detail.cid + "&qn=" + std::to_string(requested_qn) +
                                              "&fnval=" + std::to_string(setting_video_format) +
                                              "&fourk=1&fnver=0";
                if (fetch_feed(play_path, play_body, play_error)) {
                    parse_play_source(play_body, detail.play_url, detail.play_format, setting_video_codec, &detail.play_audio_url);
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_detail = detail;
            pending_detail_index = video_index;
            pending_detail_part_index = part_index;
            pending_detail_epoch = request_epoch;
            pending_detail_error = std::move(error);
            detail_fetch_in_flight = false;
            detail_fetch_complete = true;
        }
        std::thread([detail, video_index, request_epoch] {
            Video auxiliary;
            auxiliary.bvid = detail.bvid;
            auto get_comments = [detail] {
                std::vector<VideoComment> result;
                if (detail.aid.empty()) return result;
                std::string body;
                std::string error;
                const std::string path = "/x/v2/reply/wbi/main?oid=" + detail.aid +
                                         "&type=1&mode=3&next=0&ps=4";
                if (fetch_feed(path, body, error)) result = parse_video_comments(body);
                return result;
            };
            auto get_related = [detail] {
                std::vector<RelatedVideo> result;
                std::string body;
                std::string error;
                if (fetch_raw_bilibili_path("/x/web-interface/archive/related?bvid=" + url_encode(detail.bvid),
                                            body, error)) {
                    result = parse_related_videos(body);
                }
                return result;
            };
#ifdef WILIWILI_BOARD
            auxiliary.comments = get_comments();
            auxiliary.related = get_related();
#else
            auto comments = std::async(std::launch::async, std::move(get_comments));
            auto related = std::async(std::launch::async, std::move(get_related));
            auxiliary.comments = comments.get();
            auxiliary.related = related.get();
#endif
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_detail_aux = std::move(auxiliary);
            pending_detail_aux_index = video_index;
            pending_detail_aux_epoch = request_epoch;
            detail_aux_fetch_complete = true;
        }).detach();
    }).detach();
}

void select_next_video_part(lv_event_t*) {
    if (active_video_index >= displayed_videos.size()) return;
    const Video& video = displayed_videos[active_video_index];
    if (video.parts.size() < 2) return;
    request_video_detail(active_video_index, (active_part_index + 1) % video.parts.size());
}

void load_more_video_comments(lv_event_t*) {
    if (active_video_index >= displayed_videos.size()) return;
    const Video& video = displayed_videos[active_video_index];
    if (video.aid.empty() || video.comments_exhausted || video.comments.size() >= kMaxVideoComments ||
        comments_more_fetch_in_flight.exchange(true)) return;
    const int page = comment_next_page;
    const uint32_t request_epoch = navigation_epoch.load();
    std::thread([aid = video.aid, bvid = video.bvid, page, index = active_video_index, request_epoch] {
        std::string body;
        CommentPageResult result;
        result.bvid = bvid;
        result.video_index = index;
        result.epoch = request_epoch;
        result.page = page;
        const std::string path = "/x/v2/reply/wbi/main?oid=" + aid +
                                 "&type=1&mode=3&next=" + std::to_string(page) + "&ps=8";
        if (fetch_feed(path, body, result.error)) result.comments = parse_video_comments(body);
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            pending_more_comments = std::move(result);
        }
        comments_more_fetch_complete = true;
    }).detach();
    if (status_label) lv_label_set_text(status_label, "Loading more comments...");
}

void select_player_side_panel(lv_event_t* event) {
    player_side_panel = static_cast<PlayerSidePanel>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
    show_player_page(active_video_index);
}

void open_related_video(lv_event_t* event) {
    const size_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    if (active_video_index >= displayed_videos.size()) return;
    const Video& active = displayed_videos[active_video_index];
    if (index >= active.related.size() || active.related[index].bvid.empty()) return;
    Video video;
    video.bvid = active.related[index].bvid;
    video.title = active.related[index].title;
    video.owner = active.related[index].owner;
    video.view = active.related[index].view;
    displayed_videos.clear();
    displayed_videos.push_back(std::move(video));
    active_video_index = 0;
    active_part_index = 0;
    player_page_visible = false;
    open_player_after_detail = false;
    detail_return_to_season = false;
    detail_return_to_dynamic = false;
    platform_player_stop();
    player_playing = false;
    request_video_detail(0);
}

void like_dynamic_item(lv_event_t* event) {
    const size_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    if (index >= displayed_dynamic_items.size()) return;
    const std::string csrf = cookie_field("bili_jct");
    const std::string id = displayed_dynamic_items[index].id;
    if (csrf.empty() || id.empty()) {
        if (status_label) lv_label_set_text(status_label, "Dynamic like requires a signed-in Cookie");
        return;
    }
    std::thread([csrf, id] {
        std::vector<uint8_t> body;
        std::string error;
        const std::string form = "dynamic_id=" + url_encode(id) + "&up=1&csrf=" + url_encode(csrf);
        platform_https_post("https://api.vc.bilibili.com/dynamic_like/v1/dynamic_like/thumb",
                            bilibili_headers(), form, body, error);
    }).detach();
    if (status_label) lv_label_set_text(status_label, "Dynamic like request sent");
}

void open_dynamic_item(lv_event_t* event) {
    const size_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)) - 1;
    if (index >= displayed_dynamic_items.size()) return;
    const DynamicItem& item = displayed_dynamic_items[index];
    const bool valid_bvid = item.bvid.size() >= 10 && item.bvid.size() <= 16 &&
                            item.bvid.compare(0, 2, "BV") == 0 &&
                            std::all_of(item.bvid.begin(), item.bvid.end(), [](unsigned char character) {
                                return (character >= 'A' && character <= 'Z') ||
                                       (character >= 'a' && character <= 'z') ||
                                       (character >= '0' && character <= '9');
                            });
    if (valid_bvid) {
        if (detail_fetch_in_flight) return;
        Video video;
        video.bvid = item.bvid;
        video.title = item.title;
        displayed_videos.clear();
        displayed_videos.push_back(std::move(video));
        active_video_index = 0;
        active_part_index = 0;
        open_player_after_detail = false;
        detail_return_to_season = false;
        detail_return_to_dynamic = true;
        request_video_detail(0);
        return;
    }
    if (!item.id.empty() && item.id.size() <= 32 &&
        std::all_of(item.id.begin(), item.id.end(), [](unsigned char character) {
            return character >= '0' && character <= '9';
        })) {
        platform_open_external_url("https://www.bilibili.com/opus/" + item.id);
    }
}

void select_player_video_part(lv_event_t* event) {
    if (active_video_index >= displayed_videos.size()) return;
    const Video& video = displayed_videos[active_video_index];
    const size_t part_index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    if (part_index >= video.parts.size()) return;
    request_video_detail(active_video_index, part_index);
}

void open_video(lv_event_t* event) {
    const size_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)) - 1;
    // A card tap is always detail-first, never a carry-over player request.
    open_player_after_detail = false;
    detail_return_to_season = false;
    detail_return_to_dynamic = false;
    request_video_detail(index);
}

void build_video_card(lv_obj_t* parent, const Video& video, size_t index) {
    lv_obj_t* card = lv_btn_create(parent);
    lv_obj_set_size(card, 260, 238);
    lv_obj_set_style_bg_color(card, ui_color(0x1d2330), 0);
    lv_obj_set_style_radius(card, 6, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    // A feed card is a fixed tile, not a scroll target.  Keep mouse-wheel
    // input from producing an idle scrollbar on the tile itself.
    lock_static_canvas(card);
    lv_obj_add_event_cb(card, open_video, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(index + 1)));

    lv_obj_t* preview = lv_obj_create(card);
    lv_obj_set_size(preview, 240, 135);
    lv_obj_align(preview, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_color(preview, ui_color(0x283348), 0);
    lv_obj_set_style_radius(preview, 6, 0);
    lv_obj_set_style_border_width(preview, 0, 0);
    lock_static_canvas(preview);
    lv_obj_clear_flag(preview, LV_OBJ_FLAG_CLICKABLE);
    if (index < thumbnail_descriptors.size() && thumbnail_descriptors[index].data) {
        lv_obj_t* image = lv_img_create(preview);
        lv_img_set_src(image, &thumbnail_descriptors[index]);
        lv_img_set_zoom(image, (240 * LV_IMG_ZOOM_NONE) / 186);
        lv_obj_center(image);
        lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
    } else {
        set_label(preview, video.bvid.c_str(), 16, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    }

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, video.title.c_str());
    lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(title, 240);
    lv_obj_set_height(title, 42);
    lv_obj_set_style_text_font(title, ui_font(), 0);
    lv_obj_set_style_text_color(title, ui_color(0xf3f5f8), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 158);

    lv_obj_t* owner = lv_label_create(card);
    lv_label_set_text(owner, video.owner.c_str());
    lv_obj_set_size(owner, 132, 20);
    lv_label_set_long_mode(owner, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(owner, ui_font(), 0);
    lv_obj_set_style_text_color(owner, ui_color(0x9dacbd), 0);
    lv_obj_align(owner, LV_ALIGN_BOTTOM_LEFT, 10, -12);
    lv_obj_t* views = lv_label_create(card);
    const std::string view_text = video.view + " views";
    lv_label_set_text(views, view_text.c_str());
    lv_obj_set_size(views, 98, 20);
    lv_label_set_long_mode(views, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(views, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(views, ui_color(0x8bc8e8), 0);
    lv_obj_align(views, LV_ALIGN_BOTTOM_RIGHT, -10, -12);
}

void show_recommend_page(const std::vector<Video>& videos, const std::string& error) {
    if (player_page_visible || player_playing) leave_player_page();
    detail_visible = false;
    season_page_visible = false;
    if (board_preview) { show_board_home(videos, error); return; }
    if (content) lv_obj_del(content);
    detail_visible = false;
    displayed_videos = videos;
    content = lv_obj_create(lv_scr_act());
    lv_obj_set_size(content, 1120, 590);
    lv_obj_set_pos(content, 130, 110);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW_WRAP);
    // Two recommendation rows should occupy the full browsing canvas.  Space
    // tracks evenly so unused height is not stranded below the second row.
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_SPACE_EVENLY);
    if (!error.empty()) {
        lv_label_set_text(status_label, error.c_str());
        return;
    }
    lv_label_set_text(status_label, popular_selected ? "Bilibili Popular" : "Bilibili Home Recommend");
    for (size_t i = 0; i < displayed_videos.size(); ++i) {
        build_video_card(content, displayed_videos[i], i);
    }
}

void build_search_result_card(lv_obj_t* parent, const Video& video, size_t index,
                              int width, int height, int preview_width, int preview_height) {
    lv_obj_t* card = lv_btn_create(parent);
    lv_obj_set_size(card, width, height);
    lv_obj_set_style_bg_color(card, ui_color(0x1d2330), 0);
    lv_obj_set_style_radius(card, 5, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lock_static_canvas(card);
    lv_obj_add_event_cb(card, open_video, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(index + 1)));
    lv_obj_t* preview = lv_obj_create(card);
    lv_obj_set_size(preview, preview_width ? preview_width : width - 20, preview_height);
    lv_obj_set_pos(preview, 10, 10);
    lv_obj_set_style_bg_color(preview, ui_color(0x283348), 0);
    lv_obj_set_style_radius(preview, 4, 0);
    lv_obj_set_style_border_width(preview, 0, 0);
    lock_static_canvas(preview);
    lv_obj_clear_flag(preview, LV_OBJ_FLAG_CLICKABLE);
    if (index < thumbnail_descriptors.size() && thumbnail_descriptors[index].data) {
        lv_obj_t* image = lv_img_create(preview);
        lv_img_set_src(image, &thumbnail_descriptors[index]);
        lv_img_set_zoom(image, (preview_width * LV_IMG_ZOOM_NONE) / 186);
        lv_obj_center(image);
        lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, video.title.c_str());
    // Search-result titles are deliberately one bounded line.  A wrapped
    // upstream title may be arbitrarily long and otherwise overlaps the
    // stable author/view row below on compact cards.
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_size(title, width - 20, 24);
    lv_obj_set_style_text_font(title, ui_font(), 0);
    lv_obj_set_style_text_color(title, ui_color(0xf3f5f8), 0);
    lv_obj_set_pos(title, 10, preview_height + 17);
    const std::string meta = video.owner + "  " + video.view + " views";
    lv_obj_t* detail = lv_label_create(card);
    lv_label_set_text(detail, meta.c_str());
    lv_label_set_long_mode(detail, LV_LABEL_LONG_DOT);
    lv_obj_set_size(detail, width - 20, 22);
    lv_obj_set_style_text_font(detail, ui_font(), 0);
    lv_obj_set_style_text_color(detail, ui_color(0xaeb8c6), 0);
    lv_obj_set_pos(detail, 10, height - 26);
}

void show_search_results_page(const std::vector<Video>& videos, const std::string& error) {
    if (player_page_visible || player_playing) leave_player_page();
    if (nav_rail) lv_obj_clear_flag(nav_rail, LV_OBJ_FLAG_HIDDEN);
    for (lv_obj_t* control : home_header_controls) {
        if (control) lv_obj_add_flag(control, LV_OBJ_FLAG_HIDDEN);
    }
    if (content) lv_obj_del(content);
    detail_visible = false;
    displayed_videos = videos;
    content = lv_obj_create(lv_scr_act());
    const int content_width = board_preview ? 720 : 1120;
    const int content_height = board_preview ? 366 : 590;
    lv_obj_set_size(content, content_width, content_height);
    lv_obj_set_pos(content, board_preview ? 72 : 130, board_preview ? 104 : 110);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lock_static_canvas(content);
    if (top_title) lv_label_set_text(top_title, "Search");
    const std::string heading = "Search: " + search_term;
    lv_label_set_text(status_label, heading.c_str());

    // SearchTab's source order is Video, Bangumi, Cinema, Hots, History.
    // Only the public video endpoint is wired here; the remaining tabs retain
    // their source-defined identities instead of displaying fabricated data.
    const char* tabs[] = {"Video", "Bangumi", "Cinema", "Hots", "History"};
    const int tab_width = board_preview ? 82 : 116;
    for (size_t i = 0; i < sizeof(tabs) / sizeof(tabs[0]); ++i) {
        lv_obj_t* tab = lv_btn_create(content);
        lv_obj_set_size(tab, tab_width, board_preview ? 30 : 38);
        lv_obj_set_pos(tab, static_cast<int>(i) * (tab_width + (board_preview ? 6 : 10)), 0);
        lv_obj_set_style_bg_color(tab, ui_color(i == 0 ? 0x2877a9 : 0x374152), 0);
        lv_obj_set_style_radius(tab, 4, 0);
        lv_obj_set_style_border_width(tab, 0, 0);
        lv_obj_add_event_cb(tab, select_search_category, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
        set_label(tab, tabs[i], board_preview ? 12 : 15, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    }
    lv_obj_t* more = lv_btn_create(content);
    lv_obj_set_size(more, tab_width, board_preview ? 30 : 38);
    lv_obj_set_pos(more, static_cast<int>(sizeof(tabs) / sizeof(tabs[0])) * (tab_width + (board_preview ? 6 : 10)), 0);
    lv_obj_set_style_bg_color(more, ui_color(0x374152), 0);
    lv_obj_set_style_radius(more, 4, 0);
    lv_obj_set_style_border_width(more, 0, 0);
    lv_obj_add_event_cb(more, load_more_feed, LV_EVENT_CLICKED, nullptr);
    set_label(more, "More", board_preview ? 12 : 15, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    if (!error.empty()) {
        const bool request_rejected = error.find("412") != std::string::npos;
        lv_obj_t* empty = lv_obj_create(content);
        lv_obj_set_size(empty, content_width, content_height - (board_preview ? 42 : 54));
        lv_obj_set_pos(empty, 0, board_preview ? 42 : 54);
        lv_obj_set_style_bg_color(empty, ui_color(0x1d2330), 0);
        lv_obj_set_style_radius(empty, board_preview ? 6 : 8, 0);
        lv_obj_set_style_border_width(empty, 0, 0);
        lock_static_canvas(empty);
        lv_obj_t* badge = lv_obj_create(empty);
        lv_obj_set_size(badge, board_preview ? 46 : 62, board_preview ? 46 : 62);
        lv_obj_align(badge, LV_ALIGN_CENTER, 0, board_preview ? -42 : -64);
        lv_obj_set_style_bg_color(badge, ui_color(request_rejected ? 0x4b3540 : 0x203d55), 0);
        lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(badge, 0, 0);
        lock_static_canvas(badge);
        set_label(badge, request_rejected ? "!" : "?", board_preview ? 20 : 26,
                  ui_color(request_rejected ? 0xffa8b7 : 0x63c4f4), LV_ALIGN_CENTER, 0, 0);
        const char* title = request_rejected ? "Search request was limited" : "Search is unavailable";
        const char* detail = request_rejected ? "Bilibili rejected this request. Try again shortly or use another query."
                                              : "The search service did not return results. Try again shortly.";
        set_label(empty, title, board_preview ? 16 : 21, ui_color(0xf3f5f8), LV_ALIGN_CENTER, 0,
                  board_preview ? 0 : 8);
        set_label(empty, detail, board_preview ? 12 : 15, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0,
                  board_preview ? 26 : 42);
        return;
    }
    if (displayed_videos.empty()) {
        set_label(content, "No video results", board_preview ? 14 : 18, ui_color(0xaeb8c6),
                  LV_ALIGN_TOP_LEFT, 0, board_preview ? 48 : 60);
        return;
    }
    lv_obj_t* results = lv_obj_create(content);
    lv_obj_set_size(results, content_width, content_height - (board_preview ? 42 : 54));
    lv_obj_set_pos(results, 0, board_preview ? 42 : 54);
    lv_obj_set_style_bg_opa(results, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(results, 0, 0);
    lv_obj_set_style_pad_all(results, 0, 0);
    lv_obj_set_scroll_dir(results, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(results, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_flex_flow(results, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(results, board_preview ? 12 : 16, 0);
    lv_obj_set_style_pad_row(results, board_preview ? 10 : 16, 0);
    for (size_t i = 0; i < displayed_videos.size(); ++i) {
        if (board_preview) build_search_result_card(results, displayed_videos[i], i, 354, 160, 156, 88);
        else build_search_result_card(results, displayed_videos[i], i, 264, 250, 244, 137);
    }
}

void select_search_category(lv_event_t* event) {
    const size_t category = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    if (category == 4) {
        show_search_history_page();
        return;
    }
    if (category == 3) {
        fetch_search_hots();
        return;
    }
    if (category == 1) {
        fetch_media_search("media_bangumi");
        return;
    }
    if (category == 2) {
        fetch_media_search("media_ft");
        return;
    }
    // Video results are already the active page. The remaining source-defined
    // categories are handled above through their own real API routes.
    if (status_label) lv_label_set_text(status_label, ("Search: " + search_term).c_str());
}

void show_board_home(const std::vector<Video>& videos, const std::string& error) {
    if (content) lv_obj_del(content);
    displayed_videos = videos;
    content = lv_obj_create(lv_scr_act());
    lv_obj_set_size(content, 720, 374);
    lv_obj_set_pos(content, 72, 96);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(content, 12, 0);
    lv_obj_set_style_pad_row(content, 12, 0);
    if (!error.empty()) { lv_label_set_text(status_label, error.c_str()); return; }
    lv_label_set_text(status_label, "Recommended");
    for (size_t i = 0; i < displayed_videos.size() && i < 4; ++i) {
        lv_obj_t* card = lv_btn_create(content);
        lv_obj_set_size(card, 350, 168);
        lv_obj_set_style_bg_color(card, ui_color(0x182231), 0);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lock_static_canvas(card);
        lv_obj_add_event_cb(card, open_video, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(i + 1)));
        lv_obj_t* preview = lv_obj_create(card);
        lv_obj_set_size(preview, 156, 88);
        lv_obj_set_pos(preview, 12, 40);
        lv_obj_set_style_bg_color(preview, ui_color(0x243349), 0);
        lv_obj_set_style_radius(preview, 7, 0);
        lv_obj_set_style_border_width(preview, 0, 0);
        lock_static_canvas(preview);
        lv_obj_clear_flag(preview, LV_OBJ_FLAG_CLICKABLE);
        if (i < thumbnail_descriptors.size() && thumbnail_descriptors[i].data) {
            lv_obj_t* image = lv_img_create(preview);
            lv_img_set_src(image, &thumbnail_descriptors[i]);
            lv_img_set_zoom(image, (156 * LV_IMG_ZOOM_NONE) / 186);
            lv_obj_center(image);
            lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
        }
        lv_obj_t* title = lv_label_create(card);
        lv_label_set_text(title, displayed_videos[i].title.c_str());
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
        lv_obj_set_width(title, 152);
        lv_obj_set_height(title, 48);
        lv_obj_set_style_text_font(title, ui_font(), 0);
        lv_obj_set_style_text_color(title, ui_color(0xf3f5f8), 0);
        lv_obj_set_pos(title, 186, 28);
        lv_obj_t* owner = lv_label_create(card);
        lv_label_set_text(owner, displayed_videos[i].owner.c_str());
        lv_label_set_long_mode(owner, LV_LABEL_LONG_DOT);
        lv_obj_set_size(owner, 152, 20);
        lv_obj_set_style_text_font(owner, ui_font(), 0);
        lv_obj_set_style_text_color(owner, ui_color(0xaeb8c6), 0);
        lv_obj_set_pos(owner, 186, 88);
        lv_obj_t* views = lv_label_create(card);
        const std::string views_text = displayed_videos[i].view + " views";
        lv_label_set_text(views, views_text.c_str());
        lv_obj_set_style_text_font(views, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(views, ui_color(0xc8d4df), 0);
        lv_obj_set_pos(views, 186, 120);
    }
}

void update_main_nav_style() {
    for (size_t i = 0; i < 5; ++i) {
        if (!main_nav[i]) continue;
        const bool active = static_cast<size_t>(main_section) == i;
        lv_obj_set_style_bg_color(main_nav[i], ui_color(active ? (board_preview ? 0x0ea5e9 : 0x2877a9) :
                                                                       (board_preview ? 0x1f2937 : 0x2b3444)), 0);
    }
}

void update_shell_for_section(MainSection section) {
    const bool home = section == MainSection::Home;
    for (lv_obj_t* control : home_header_controls) {
        if (!control) continue;
        if (home) lv_obj_clear_flag(control, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(control, LV_OBJ_FLAG_HIDDEN);
    }
    const char* names[] = {"Home", "Dynamic", "Mine", "Inbox", "Settings"};
    if (top_title) lv_label_set_text(top_title, names[static_cast<size_t>(section)]);
    if (status_label) {
        if (home) lv_obj_clear_flag(status_label, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void update_dynamic_tab_style() {
    if (!dynamic_video_tab || !dynamic_article_tab) return;
    lv_obj_set_style_bg_color(dynamic_video_tab,
                              ui_color(dynamic_showing_articles ? 0x374152 : 0x2877a9), 0);
    lv_obj_set_style_bg_color(dynamic_article_tab,
                              ui_color(dynamic_showing_articles ? 0x2877a9 : 0x374152), 0);
}

void select_dynamic_tab(lv_event_t* event) {
    ++dynamic_request_epoch;
    selected_dynamic_mid.clear();
    dynamic_showing_articles = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)) != 0;
    dynamic_fetch_requested = false;
    dynamic_article_page = 1;
    dynamic_video_offset.clear();
    displayed_dynamic_items.clear();
    if (board_preview) show_main_section(MainSection::Dynamic);
    else show_dynamic_page();
}

void select_dynamic_up(lv_event_t* event) {
    const size_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    if (index >= displayed_dynamic_ups.size() || displayed_dynamic_ups[index].mid.empty()) return;
    selected_dynamic_mid = displayed_dynamic_ups[index].mid;
    ++dynamic_request_epoch;
    dynamic_showing_articles = true;
    dynamic_article_page = 1;
    dynamic_fetch_requested = false;
    displayed_dynamic_items.clear();
    show_dynamic_page();
}

void select_all_dynamic(lv_event_t*) {
    selected_dynamic_mid.clear();
    ++dynamic_request_epoch;
    dynamic_article_page = 1;
    dynamic_video_offset.clear();
    dynamic_fetch_requested = false;
    displayed_dynamic_items.clear();
    show_dynamic_page();
}

void load_more_dynamic(lv_event_t*) {
    if (dynamic_fetch_in_flight) return;
    fetch_dynamic_items(dynamic_showing_articles, true);
    if (status_label) lv_label_set_text(status_label, "Dynamic: loading next page...");
}

void show_dynamic_page() {
    if (content) lv_obj_del(content);
    detail_visible = false;
    content = lv_obj_create(lv_scr_act());
    lv_obj_set_size(content, 1120, 590);
    lv_obj_set_pos(content, 130, 110);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    // The authenticated view needs its UP selector.  Before sign-in it only
    // consumes space and turns the page into two competing empty panels.
    if (account_name.empty()) {
        lv_obj_t* pane = lv_obj_create(content);
        lv_obj_set_size(pane, 1080, 570);
        lv_obj_set_pos(pane, 20, 0);
        lv_obj_set_style_bg_color(pane, ui_color(0x1d2330), 0);
        lv_obj_set_style_radius(pane, 8, 0);
        lv_obj_set_style_border_width(pane, 0, 0);
        lv_obj_clear_flag(pane, LV_OBJ_FLAG_SCROLLABLE);

        dynamic_video_tab = lv_btn_create(pane);
        lv_obj_set_size(dynamic_video_tab, 112, 40);
        lv_obj_set_pos(dynamic_video_tab, 28, 24);
        lv_obj_set_style_radius(dynamic_video_tab, 5, 0);
        lv_obj_set_style_border_width(dynamic_video_tab, 0, 0);
        lv_obj_add_event_cb(dynamic_video_tab, select_dynamic_tab, LV_EVENT_CLICKED, nullptr);
        set_label(dynamic_video_tab, "Videos", 16, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        dynamic_article_tab = lv_btn_create(pane);
        lv_obj_set_size(dynamic_article_tab, 112, 40);
        lv_obj_set_pos(dynamic_article_tab, 150, 24);
        lv_obj_set_style_radius(dynamic_article_tab, 5, 0);
        lv_obj_set_style_border_width(dynamic_article_tab, 0, 0);
        lv_obj_add_event_cb(dynamic_article_tab, select_dynamic_tab, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(1)));
        set_label(dynamic_article_tab, "Articles", 16, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        update_dynamic_tab_style();

        lv_obj_t* badge = lv_obj_create(pane);
        lv_obj_set_size(badge, 62, 62);
        lv_obj_align(badge, LV_ALIGN_CENTER, 0, -74);
        lv_obj_set_style_bg_color(badge, ui_color(0x203d55), 0);
        lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(badge, 0, 0);
        lock_static_canvas(badge);
        set_symbol(badge, LV_SYMBOL_LIST, ui_color(0x63c4f4));
        set_label(pane, dynamic_showing_articles ? "Following articles" : "Following videos",
                  22, ui_color(0xf3f5f8), LV_ALIGN_CENTER, 0, 0);
        set_label(pane, "Sign in with a Bilibili Cookie to load your feed.",
                  16, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 34);
        lv_label_set_text(status_label, "Dynamic: login is required");
        return;
    }

    // Mirrors dynamic_tab.xml: a fixed UP selector on the left and two
    // independent video/article panes on the right. Upstream deliberately
    // refuses its feed API when no logged-in account is available.
    lv_obj_t* following = lv_obj_create(content);
    lv_obj_set_size(following, 250, 570);
    lv_obj_set_pos(following, 0, 0);
    lv_obj_set_style_bg_color(following, ui_color(0x1d2330), 0);
    lv_obj_set_style_radius(following, 6, 0);
    lv_obj_set_style_border_width(following, 0, 0);
    lv_obj_set_style_pad_all(following, 12, 0);
    lock_static_canvas(following);
    set_label(following, "Following", 18, ui_color(0xf3f5f8), LV_ALIGN_TOP_LEFT, 4, 2);
    lv_obj_t* all_updates = lv_btn_create(following);
    lv_obj_set_size(all_updates, 226, 44);
    lv_obj_align(all_updates, LV_ALIGN_TOP_MID, 0, 42);
    lv_obj_set_style_bg_color(all_updates, ui_color(selected_dynamic_mid.empty() ? 0x2877a9 : 0x374152), 0);
    lv_obj_set_style_radius(all_updates, 5, 0);
    lv_obj_set_style_border_width(all_updates, 0, 0);
    lv_obj_add_event_cb(all_updates, select_all_dynamic, LV_EVENT_CLICKED, nullptr);
    set_label(all_updates, "All updates", 16, ui_color(0xffffff), LV_ALIGN_LEFT_MID, 14, 0);
    if (account_name.empty()) {
        set_label(following, "Sign in to load the UP list\nand following feed.",
                  14, ui_color(0xaeb8c6), LV_ALIGN_TOP_LEFT, 4, 106);
    } else if (displayed_dynamic_ups.empty()) {
        set_label(following, dynamic_up_fetch_requested && !dynamic_up_fetch_in_flight ?
                  "UP list unavailable" : "Loading UP list...", 14, ui_color(0xaeb8c6),
                  LV_ALIGN_TOP_LEFT, 4, 106);
        if (!dynamic_up_fetch_requested) fetch_dynamic_ups();
    } else {
        for (size_t index = 0; index < displayed_dynamic_ups.size(); ++index) {
            lv_obj_t* row = lv_btn_create(following);
            lv_obj_set_size(row, 226, 38);
            lv_obj_set_pos(row, 0, 102 + static_cast<int>(index) * 44);
            lv_obj_set_style_bg_color(row,
                ui_color(displayed_dynamic_ups[index].mid == selected_dynamic_mid && !selected_dynamic_mid.empty() ?
                             0x2877a9 : 0x202735), 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_radius(row, 4, 0);
            lock_static_canvas(row);
            lv_obj_add_event_cb(row, select_dynamic_up, LV_EVENT_CLICKED,
                                reinterpret_cast<void*>(static_cast<uintptr_t>(index)));
            set_label(row, displayed_dynamic_ups[index].name.c_str(), 14,
                      ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 10, 0);
        }
    }

    lv_obj_t* pane = lv_obj_create(content);
    lv_obj_set_size(pane, 850, 570);
    lv_obj_set_pos(pane, 270, 0);
    lv_obj_set_style_bg_opa(pane, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pane, 0, 0);
    lv_obj_set_style_pad_all(pane, 0, 0);
    lv_obj_clear_flag(pane, LV_OBJ_FLAG_SCROLLABLE);
    dynamic_video_tab = lv_btn_create(pane);
    lv_obj_set_size(dynamic_video_tab, 108, 40);
    lv_obj_set_pos(dynamic_video_tab, 0, 0);
    lv_obj_set_style_radius(dynamic_video_tab, 5, 0);
    lv_obj_set_style_border_width(dynamic_video_tab, 0, 0);
    lv_obj_add_event_cb(dynamic_video_tab, select_dynamic_tab, LV_EVENT_CLICKED, nullptr);
    set_label(dynamic_video_tab, "Videos", 16, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    dynamic_article_tab = lv_btn_create(pane);
    lv_obj_set_size(dynamic_article_tab, 108, 40);
    lv_obj_set_pos(dynamic_article_tab, 118, 0);
    lv_obj_set_style_radius(dynamic_article_tab, 5, 0);
    lv_obj_set_style_border_width(dynamic_article_tab, 0, 0);
    lv_obj_add_event_cb(dynamic_article_tab, select_dynamic_tab, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(1)));
    set_label(dynamic_article_tab, "Articles", 16, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    lv_obj_t* more = lv_btn_create(pane);
    lv_obj_set_size(more, 130, 40);
    lv_obj_set_pos(more, 240, 0);
    lv_obj_set_style_bg_color(more, ui_color(0x374152), 0);
    lv_obj_set_style_radius(more, 5, 0);
    lv_obj_set_style_border_width(more, 0, 0);
    lv_obj_add_event_cb(more, load_more_dynamic, LV_EVENT_CLICKED, nullptr);
    set_label(more, "Load more", 14, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    update_dynamic_tab_style();

    lv_obj_t* empty = lv_obj_create(pane);
    lv_obj_set_size(empty, 850, 500);
    lv_obj_set_pos(empty, 0, 60);
    lv_obj_set_style_bg_color(empty, ui_color(0x1d2330), 0);
    lv_obj_set_style_radius(empty, 6, 0);
    lv_obj_set_style_border_width(empty, 0, 0);
    lv_obj_set_style_pad_all(empty, 0, 0);
    lv_obj_set_scroll_dir(empty, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(empty, LV_SCROLLBAR_MODE_AUTO);
    const char* title = dynamic_showing_articles ? "Following articles" : "Following videos";
    if (account_name.empty()) {
        set_label(empty, title, 20, ui_color(0xf3f5f8), LV_ALIGN_CENTER, 0, -28);
        set_label(empty, "Sign in with a Bilibili Cookie to load this feed.",
                  16, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 28);
        lv_label_set_text(status_label, "Dynamic: login is required");
    } else if (displayed_dynamic_items.empty()) {
        const bool unavailable = dynamic_fetch_requested && !dynamic_fetch_in_flight;
        set_label(empty, unavailable ? (displayed_dynamic_error.empty() ? "No updates yet" : "Feed unavailable; select tab to retry") : "Loading dynamic feed...",
                  20, ui_color(0xf3f5f8), LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(status_label, unavailable ? "Dynamic: select tab to retry" : "Dynamic: loading authenticated feed");
        if (!dynamic_fetch_requested) fetch_dynamic_items(dynamic_showing_articles);
    } else {
        lv_label_set_text(status_label, "Dynamic: authenticated feed");
        for (size_t index = 0; index < displayed_dynamic_items.size(); ++index) {
            const DynamicItem& item = displayed_dynamic_items[index];
            lv_obj_t* row = item.bvid.empty() && item.id.empty() ? lv_obj_create(empty) : lv_btn_create(empty);
            lv_obj_set_size(row, 810, 56);
            lv_obj_set_pos(row, 18, 16 + static_cast<int>(index) * 60);
            lv_obj_set_style_bg_color(row, ui_color(0x202735), 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_radius(row, 4, 0);
            lock_static_canvas(row);
            if (!item.bvid.empty() || !item.id.empty())
                lv_obj_add_event_cb(row, open_dynamic_item, LV_EVENT_CLICKED,
                                    reinterpret_cast<void*>(static_cast<uintptr_t>(index + 1)));
            dynamic_row_line(row, item.title, 15, ui_color(0xf3f5f8), 670, 5);
            if (!item.summary.empty())
                dynamic_row_line(row, item.summary, 12, ui_color(0xaeb8c6), 670, 29);
            if (!item.id.empty()) {
                lv_obj_t* like = lv_btn_create(row);
                lv_obj_set_size(like, 54, 28);
                lv_obj_align(like, LV_ALIGN_RIGHT_MID, -8, 0);
                lv_obj_add_event_cb(like, like_dynamic_item, LV_EVENT_CLICKED,
                                    reinterpret_cast<void*>(static_cast<uintptr_t>(index)));
                set_label(like, "Like", 11, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
            }
        }
    }
}

void select_mine_tab(lv_event_t* event) {
    mine_active_tab = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
    if (mine_active_tab < mine_fetch_requested.size()) mine_fetch_requested[mine_active_tab] = false;
    if (board_preview) show_main_section(MainSection::Mine);
    else show_mine_page();
}

void open_mine_video(lv_event_t* event) {
    const uintptr_t packed = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    const unsigned int source = static_cast<unsigned int>(packed >> 16);
    const size_t index = static_cast<size_t>(packed & 0xffffu);
    const std::vector<Video>* list = source == 0 ? &displayed_history_videos :
                                     source == 1 ? &displayed_favorite_videos : &displayed_later_videos;
    if (index >= list->size()) return;
    displayed_videos.clear();
    displayed_videos.push_back((*list)[index]);
    active_video_index = 0;
    open_player_after_detail = false;
    detail_return_to_dynamic = false;
    request_video_detail(0);
}

void open_qr_login(lv_event_t*) {
    fetch_qr_login();
    if (status_label) lv_label_set_text(status_label, "QR login: requesting code...");
}

void logout_account(lv_event_t*) {
    ++dynamic_request_epoch;
    selected_dynamic_mid.clear();
    bilibili_cookie.clear();
    save_cookie();
    mine_fetch_requested.fill(false);
    inbox_fetch_requested.fill(false);
    dynamic_fetch_requested = false;
    dynamic_up_fetch_requested = false;
    account_name.clear();
    account_stats = {};
    displayed_history_videos.clear();
    displayed_favorite_videos.clear();
    displayed_later_videos.clear();
    displayed_subscription_titles.clear();
    displayed_bangumi_titles.clear();
    displayed_series_titles.clear();
    displayed_dynamic_items.clear();
    displayed_dynamic_ups.clear();
    displayed_inbox_items.clear();
    if (board_preview) show_main_section(MainSection::Mine);
    else show_mine_page();
}

void show_mine_page() {
    if (content) lv_obj_del(content);
    detail_visible = false;
    content = lv_obj_create(lv_scr_act());
    lv_obj_set_size(content, 1120, 590);
    lv_obj_set_pos(content, 130, 110);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    // Layout and tab order follow fragment/mine_tab.xml. Its presenter starts
    // in onUserNotLogin() until QR login/cookie persistence is migrated.
    lv_obj_t* user = lv_obj_create(content);
    lv_obj_set_size(user, 1080, 100);
    lv_obj_set_pos(user, 20, 0);
    lv_obj_set_style_bg_color(user, ui_color(0x1d2330), 0);
    lv_obj_set_style_radius(user, 6, 0);
    lv_obj_set_style_border_width(user, 0, 0);
    lv_obj_t* avatar = lv_obj_create(user);
    lv_obj_set_size(avatar, 76, 76);
    lv_obj_align(avatar, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_set_style_bg_color(avatar, ui_color(0x374152), 0);
    lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(avatar, 0, 0);
    set_label(avatar, "@", 26, ui_color(0xdce8f3), LV_ALIGN_CENTER, 0, 0);
    const bool signed_in = !account_name.empty();
    set_label(user, signed_in ? account_name.c_str() : "Click to sign in", 20,
              ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 108, -14);
    set_label(user, signed_in ? "Cookie session is active" : "Import a Cookie session to sign in", 15,
              ui_color(0xaeb8c6), LV_ALIGN_LEFT_MID, 108, 18);
    if (!signed_in) {
        lv_obj_t* login = lv_btn_create(user);
        lv_obj_set_size(login, 150, 42);
        lv_obj_align(login, LV_ALIGN_RIGHT_MID, -20, 0);
        lv_obj_add_event_cb(login, open_qr_login, LV_EVENT_CLICKED, nullptr);
        set_label(login, "Show QR", 14, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    } else {
        lv_obj_t* logout = lv_btn_create(user);
        lv_obj_set_size(logout, 150, 42);
        lv_obj_align(logout, LV_ALIGN_RIGHT_MID, -20, 0);
        lv_obj_add_event_cb(logout, logout_account, LV_EVENT_CLICKED, nullptr);
        set_label(logout, "Sign out", 14, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    }
    const char* stat_labels[] = {"Dynamic", "Following", "Followers", "Coins"};
    if (signed_in) {
        // Statistics belong to an authenticated profile.  Keeping zero-value
        // placeholders in the signed-out card made them collide with login.
        for (int i = 0; i < 4; ++i) {
            const int x = 570 + i * 84;
            set_label(user, !account_stats[i].empty() ? account_stats[i].c_str() : "0",
                      18, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, x, -14);
            set_label(user, stat_labels[i], 12, ui_color(0xaeb8c6), LV_ALIGN_LEFT_MID, x, 18);
        }
    }

    const char* tabs[] = {"History", "Favorites", "Subscription", "Anime", "Series", "Watch later"};
    mine_tabs.clear();
    for (unsigned int i = 0; i < 6; ++i) {
        lv_obj_t* tab = lv_btn_create(content);
        lv_obj_set_size(tab, 170, 40);
        lv_obj_set_pos(tab, 20 + static_cast<int>(i) * 180, 118);
        lv_obj_set_style_bg_color(tab, ui_color(i == mine_active_tab ? 0x2877a9 : 0x374152), 0);
        lv_obj_set_style_radius(tab, 5, 0);
        lv_obj_set_style_border_width(tab, 0, 0);
        lv_obj_add_event_cb(tab, select_mine_tab, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
        set_label(tab, tabs[i], 15, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        mine_tabs.push_back(tab);
    }
    lv_obj_t* pane = lv_obj_create(content);
    lv_obj_set_size(pane, 1120, 410);
    lv_obj_set_pos(pane, 0, 170);
    lv_obj_set_style_bg_color(pane, ui_color(0x1d2330), 0);
    lv_obj_set_style_radius(pane, 6, 0);
    lv_obj_set_style_border_width(pane, 0, 0);
    set_label(pane, tabs[mine_active_tab], 21, ui_color(0xf3f5f8), LV_ALIGN_TOP_LEFT, 24, 18);
    if (!signed_in) {
        lv_obj_t* badge = lv_obj_create(pane);
        lv_obj_set_size(badge, 62, 62);
        lv_obj_align(badge, LV_ALIGN_CENTER, 0, -60);
        lv_obj_set_style_bg_color(badge, ui_color(0x203d55), 0);
        lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(badge, 0, 0);
        lock_static_canvas(badge);
        set_label(badge, "@", 24, ui_color(0x63c4f4), LV_ALIGN_CENTER, 0, 0);
        set_label(pane, "Sign in to view your library", 20, ui_color(0xf3f5f8), LV_ALIGN_CENTER, 0, 14);
        set_label(pane, "History, favorites, and subscriptions will appear here.",
                  15, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 46);
    } else if (mine_active_tab == 0 && displayed_history_videos.empty()) {
        set_label(pane, mine_fetch_requested[0] && !history_fetch_in_flight ? "No history or unavailable" :
                  "Loading watch history...", 16, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 28);
        if (!mine_fetch_requested[0]) fetch_history_videos();
    } else if (mine_active_tab == 0 && !displayed_history_videos.empty()) {
        for (size_t index = 0; index < displayed_history_videos.size(); ++index) {
            const Video& item = displayed_history_videos[index];
            lv_obj_t* row = lv_obj_create(pane);
            lv_obj_set_size(row, 1060, 48);
            lv_obj_set_pos(row, 24, 60 + static_cast<int>(index) * 54);
            lv_obj_set_style_bg_color(row, ui_color(0x202735), 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_radius(row, 4, 0);
            lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(row, open_mine_video, LV_EVENT_CLICKED,
                                reinterpret_cast<void*>(static_cast<uintptr_t>(index)));
            set_label(row, item.title.c_str(), 15, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 12, 0);
            set_label(row, item.owner.empty() ? item.bvid.c_str() : item.owner.c_str(),
                      12, ui_color(0xaeb8c6), LV_ALIGN_RIGHT_MID, -12, 0);
        }
    } else if (mine_active_tab == 1 && displayed_favorite_videos.empty()) {
        set_label(pane, mine_fetch_requested[1] && !favorite_fetch_in_flight ? "No favorites or unavailable" :
                  "Loading favorites...", 16, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 28);
        if (!mine_fetch_requested[1]) fetch_favorite_videos();
    } else if (mine_active_tab == 1 && !displayed_favorite_videos.empty()) {
        for (size_t index = 0; index < displayed_favorite_videos.size(); ++index) {
            const Video& item = displayed_favorite_videos[index];
            lv_obj_t* row = lv_obj_create(pane);
            lv_obj_set_size(row, 1060, 48);
            lv_obj_set_pos(row, 24, 60 + static_cast<int>(index) * 54);
            lv_obj_set_style_bg_color(row, ui_color(0x202735), 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_radius(row, 4, 0);
            lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(row, open_mine_video, LV_EVENT_CLICKED,
                                reinterpret_cast<void*>(static_cast<uintptr_t>((1u << 16) | index)));
            set_label(row, item.title.c_str(), 15, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 12, 0);
            set_label(row, item.bvid.c_str(), 12, ui_color(0xaeb8c6), LV_ALIGN_RIGHT_MID, -12, 0);
            lv_obj_t* remove = lv_btn_create(row);
            lv_obj_set_size(remove, 76, 30);
            lv_obj_align(remove, LV_ALIGN_RIGHT_MID, -10, 0);
            lv_obj_set_style_bg_color(remove, ui_color(0x8d3b45), 0);
            lv_obj_set_style_border_width(remove, 0, 0);
            lv_obj_add_event_cb(remove, remove_favorite_video, LV_EVENT_CLICKED,
                                reinterpret_cast<void*>(static_cast<uintptr_t>(index)));
            set_label(remove, "Remove", 11, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        }
    } else if (mine_active_tab == 5 && displayed_later_videos.empty()) {
        set_label(pane, mine_fetch_requested[5] && !later_fetch_in_flight ? "No watch later or unavailable" :
                  "Loading watch later...", 16, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 28);
        if (!mine_fetch_requested[5]) fetch_later_videos();
    } else if (mine_active_tab == 5 && !displayed_later_videos.empty()) {
        for (size_t index = 0; index < displayed_later_videos.size(); ++index) {
            const Video& item = displayed_later_videos[index];
            lv_obj_t* row = lv_obj_create(pane);
            lv_obj_set_size(row, 1060, 48);
            lv_obj_set_pos(row, 24, 60 + static_cast<int>(index) * 54);
            lv_obj_set_style_bg_color(row, ui_color(0x202735), 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_radius(row, 4, 0);
            lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(row, open_mine_video, LV_EVENT_CLICKED,
                                reinterpret_cast<void*>(static_cast<uintptr_t>((2u << 16) | index)));
            set_label(row, item.title.c_str(), 15, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 12, 0);
            set_label(row, item.bvid.c_str(), 12, ui_color(0xaeb8c6), LV_ALIGN_RIGHT_MID, -12, 0);
            lv_obj_t* remove = lv_btn_create(row);
            lv_obj_set_size(remove, 76, 30);
            lv_obj_align(remove, LV_ALIGN_RIGHT_MID, -10, 0);
            lv_obj_set_style_bg_color(remove, ui_color(0x8d3b45), 0);
            lv_obj_set_style_border_width(remove, 0, 0);
            lv_obj_add_event_cb(remove, remove_later_video, LV_EVENT_CLICKED,
                                reinterpret_cast<void*>(static_cast<uintptr_t>(index)));
            set_label(remove, "Remove", 11, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        }
    } else if (mine_active_tab == 2 && displayed_subscription_titles.empty()) {
        set_label(pane, mine_fetch_requested[2] && !subscription_fetch_in_flight ? "No subscriptions or unavailable" :
                  "Loading subscriptions...", 16, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 28);
        if (!mine_fetch_requested[2]) fetch_subscriptions();
    } else if (mine_active_tab == 2) {
        for (size_t index = 0; index < displayed_subscription_titles.size(); ++index) {
            lv_obj_t* row = lv_obj_create(pane);
            lv_obj_set_size(row, 1060, 48);
            lv_obj_set_pos(row, 24, 60 + static_cast<int>(index) * 54);
            lv_obj_set_style_bg_color(row, ui_color(0x202735), 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_radius(row, 4, 0);
            set_label(row, displayed_subscription_titles[index].c_str(), 15,
                      ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 12, 0);
        }
    } else if (mine_active_tab == 3 && displayed_bangumi_titles.empty()) {
        set_label(pane, mine_fetch_requested[3] && !bangumi_fetch_in_flight ? "No anime or unavailable" :
                  "Loading anime...", 16, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 28);
        if (!mine_fetch_requested[3]) fetch_bangumi_titles();
    } else if (mine_active_tab == 3) {
        for (size_t index = 0; index < displayed_bangumi_titles.size(); ++index) {
            lv_obj_t* row = lv_obj_create(pane);
            lv_obj_set_size(row, 1060, 48);
            lv_obj_set_pos(row, 24, 60 + static_cast<int>(index) * 54);
            lv_obj_set_style_bg_color(row, ui_color(0x202735), 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_radius(row, 4, 0);
            set_label(row, displayed_bangumi_titles[index].c_str(), 15,
                      ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 12, 0);
        }
    } else if (mine_active_tab == 4 && displayed_series_titles.empty()) {
        set_label(pane, mine_fetch_requested[4] && !series_fetch_in_flight ? "No series or unavailable" :
                  "Loading series...", 16, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 28);
        if (!mine_fetch_requested[4]) fetch_series_titles();
    } else if (mine_active_tab == 4) {
        for (size_t index = 0; index < displayed_series_titles.size(); ++index) {
            lv_obj_t* row = lv_obj_create(pane);
            lv_obj_set_size(row, 1060, 48);
            lv_obj_set_pos(row, 24, 60 + static_cast<int>(index) * 54);
            lv_obj_set_style_bg_color(row, ui_color(0x202735), 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_radius(row, 4, 0);
            set_label(row, displayed_series_titles[index].c_str(), 15,
                      ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 12, 0);
        }
    } else {
        set_label(pane, "No account section selected.", 16,
                  ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 28);
    }
    const std::string status = signed_in ? "Mine: Cookie session active" :
                                           "Mine: " + std::string(tabs[mine_active_tab]) + " requires login";
    lv_label_set_text(status_label, status.c_str());
}

void select_settings_tab(lv_event_t* event) {
    settings_active_tab = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
    if (board_preview) show_main_section(MainSection::Settings);
    else show_settings_page();
}

void toggle_setting(lv_event_t* event) {
    bool* value = static_cast<bool*>(lv_event_get_user_data(event));
    if (value) {
        *value = lv_obj_has_state(lv_event_get_target(event), LV_STATE_CHECKED);
        save_lvgl_config();
    }
}

void toggle_setting_button(lv_event_t* event) {
    bool* value = static_cast<bool*>(lv_event_get_user_data(event));
    if (!value) return;
    *value = !*value;
    lv_obj_t* button = static_cast<lv_obj_t*>(lv_event_get_target(event));
    lv_obj_set_style_bg_color(button, ui_color(*value ? 0x2877a9 : 0x566273), 0);
    save_lvgl_config();
    if (value == &setting_http_proxy_enabled) {
        platform_set_http_proxy(setting_http_proxy, setting_http_proxy_enabled);
    }
    if (value == &setting_tls_verify) platform_set_tls_verify(setting_tls_verify);
    if (value == &setting_swap_abxy) platform_set_gamepad_ab_swap(setting_swap_abxy);
    if (value == &setting_hide_bottom_bar && nav_rail) {
        if (*value) lv_obj_add_flag(nav_rail, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(nav_rail, LV_OBJ_FLAG_HIDDEN);
    }
    if (value == &setting_show_fps) {
        if (*value && !fps_label) {
            fps_label = lv_label_create(lv_scr_act());
            lv_obj_set_style_text_font(fps_label, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(fps_label, ui_color(0x8fd3ff), 0);
            lv_obj_align(fps_label, LV_ALIGN_TOP_RIGHT, board_preview ? -8 : -10, 8);
            lv_label_set_text(fps_label, "FPS --");
            fps_window_start = platform_ticks();
            fps_frame_count = 0;
        } else if (!*value && fps_label) {
            lv_obj_del(fps_label);
            fps_label = nullptr;
        }
    }
    if (value == &setting_danmaku_on && *value && player_page_visible) {
        request_danmaku(active_video_index);
    }
    if (main_section == MainSection::Settings) {
        if (board_preview) show_main_section(MainSection::Settings);
        else show_settings_page();
    }
}

void apply_ui_scale() {
    if (!content || board_preview) return;
    // LVGL zoom is expressed in 1/256 units.  Keep the board profile at its
    // authored 800x480 geometry; desktop users can preview the same tree at a
    // larger or smaller scale without changing layout coordinates.
    lv_obj_set_style_transform_zoom(lv_scr_act(), static_cast<uint16_t>(setting_ui_scale * 256 / 100), 0);
}

void cycle_ui_scale(lv_event_t*) {
    static const int values[] = {75, 100, 125, 150};
    size_t index = 1;
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        if (values[i] == setting_ui_scale) { index = i; break; }
    }
    setting_ui_scale = values[(index + 1) % (sizeof(values) / sizeof(values[0]))];
    save_lvgl_config();
    apply_ui_scale();
    if (status_label) {
        const std::string status = "UI scale: " + std::to_string(setting_ui_scale) + "%";
        lv_label_set_text(status_label, status.c_str());
    }
    if (main_section == MainSection::Settings) {
        if (board_preview) show_main_section(MainSection::Settings);
        else show_settings_page();
    }
}

void cycle_limited_fps(lv_event_t*) {
    static const int options[] = {0, 30, 60, 120};
    auto it = std::find(std::begin(options), std::end(options), setting_limited_fps);
    const size_t index = it == std::end(options) ? 0 : static_cast<size_t>(it - std::begin(options));
    setting_limited_fps = options[(index + 1) % (sizeof(options) / sizeof(options[0]))];
    save_lvgl_config();
    if (main_section == MainSection::Settings) {
        if (board_preview) show_main_section(MainSection::Settings);
        else show_settings_page();
    }
}

void cycle_keymap(lv_event_t*) {
    static const char* options[] = {"xbox", "ps", "keyboard"};
    size_t index = 0;
    for (size_t candidate = 0; candidate < 3; ++candidate) {
        if (setting_keymap == options[candidate]) { index = candidate; break; }
    }
    setting_keymap = options[(index + 1) % 3];
    save_lvgl_config();
    if (main_section == MainSection::Settings) {
        if (board_preview) show_main_section(MainSection::Settings);
        else show_settings_page();
    }
}

void cycle_image_request_threads(lv_event_t*) {
    static const int options[] = {1, 2, 4, 8};
    auto it = std::find(std::begin(options), std::end(options), setting_image_threads);
    const size_t index = it == std::end(options) ? 2 : static_cast<size_t>(it - std::begin(options));
    setting_image_threads = options[(index + 1) % (sizeof(options) / sizeof(options[0]))];
    save_lvgl_config();
    if (main_section == MainSection::Settings) {
        if (board_preview) show_main_section(MainSection::Settings);
        else show_settings_page();
    }
}

void cycle_http_timeout(lv_event_t* event) {
    const bool connect = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)) != 0;
    int& value = connect ? setting_http_connect_timeout_seconds : setting_http_timeout_seconds;
    static const int options[] = {5, 10, 15, 30};
    auto it = std::find(std::begin(options), std::end(options), value);
    const size_t index = it == std::end(options) ? 1 : static_cast<size_t>(it - std::begin(options));
    value = options[(index + 1) % (sizeof(options) / sizeof(options[0]))];
    platform_set_http_timeouts(static_cast<uint32_t>(setting_http_connect_timeout_seconds * 1000),
                               static_cast<uint32_t>(setting_http_timeout_seconds * 1000));
    save_lvgl_config();
    if (main_section == MainSection::Settings) {
        if (board_preview) show_main_section(MainSection::Settings);
        else show_settings_page();
    }
}

void build_shell();

void cycle_app_theme(lv_event_t*) {
    static const char* options[] = {"auto", "light", "dark"};
    size_t index = 0;
    for (size_t candidate = 0; candidate < 3; ++candidate) {
        if (setting_app_theme == options[candidate]) { index = candidate; break; }
    }
    setting_app_theme = options[(index + 1) % 3];
    save_lvgl_config();
    if (main_section == MainSection::Settings) {
        lv_obj_clean(lv_scr_act());
        content = nullptr;
        home_header_controls.clear();
        active_text_input = nullptr;
        search_focused = false;
        proxy_input = nullptr;
        detail_comment_input = nullptr;
        inbox_message_input = nullptr;
        fps_label = nullptr;
        build_shell();
        if (board_preview) show_main_section(MainSection::Settings);
        else show_settings_page();
    }
}

void cycle_quality_setting(lv_event_t* event) {
    const uintptr_t kind = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    int* value = kind == 1 ? &setting_video_quality : kind == 2 ? &setting_audio_quality : &setting_memory_cache;
    if (kind == 1) {
        static const int options[] = {16, 32, 64, 80};
        *value = options[(std::find(std::begin(options), std::end(options), *value) - std::begin(options) + 1) % 4];
    } else if (kind == 2) {
        static const int options[] = {30216, 30232, 30280};
        *value = options[(std::find(std::begin(options), std::end(options), *value) - std::begin(options) + 1) % 3];
    } else {
        static const int options[] = {32, 64, 128, 256};
        *value = options[(std::find(std::begin(options), std::end(options), *value) - std::begin(options) + 1) % 4];
    }
    save_lvgl_config();
    if (main_section == MainSection::Settings) {
        if (board_preview) show_main_section(MainSection::Settings);
        else show_settings_page();
    }
}

void cycle_video_format(lv_event_t*) {
    setting_video_format = setting_video_format == 4048 ? 0 : 4048;
    save_lvgl_config();
    if (main_section == MainSection::Settings) {
        if (board_preview) show_main_section(MainSection::Settings);
        else show_settings_page();
    }
}

void cycle_video_codec(lv_event_t*) {
    static const int options[] = {7, 12, 13};
    auto it = std::find(std::begin(options), std::end(options), setting_video_codec);
    const size_t index = it == std::end(options) ? 0 : static_cast<size_t>(it - std::begin(options));
    setting_video_codec = options[(index + 1) % (sizeof(options) / sizeof(options[0]))];
    save_lvgl_config();
    if (main_section == MainSection::Settings) {
        if (board_preview) show_main_section(MainSection::Settings);
        else show_settings_page();
    }
}

void add_video_option_button(lv_obj_t* parent, const char* title, const char* value,
                             lv_event_cb_t callback, int y) {
    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_size(button, 730, 54);
    lv_obj_set_pos(button, 28, y);
    lv_obj_set_style_bg_color(button, ui_color(0x202735), 0);
    lv_obj_set_style_radius(button, 5, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);
    const std::string label = std::string(title) + "   " + value;
    set_label(button, label.c_str(), 15, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 16, 0);
}

void cycle_danmaku_speed(lv_event_t*) {
    setting_danmaku_speed = setting_danmaku_speed >= 200 ? 50 : setting_danmaku_speed + 25;
    save_lvgl_config();
    if (main_section == MainSection::Settings) {
        if (board_preview) show_main_section(MainSection::Settings);
        else show_settings_page();
    }
}

void cycle_danmaku_area(lv_event_t*) {
    static const int options[] = {25, 50, 75, 100};
    auto it = std::find(std::begin(options), std::end(options), setting_danmaku_area);
    const size_t index = it == std::end(options) ? 3 : static_cast<size_t>(it - std::begin(options));
    setting_danmaku_area = options[(index + 1) % (sizeof(options) / sizeof(options[0]))];
    save_lvgl_config();
    if (player_page_visible && active_video_index < displayed_videos.size()) show_player_page(active_video_index);
    else if (main_section == MainSection::Settings) {
        if (board_preview) show_main_section(MainSection::Settings);
        else show_settings_page();
    }
}

void cycle_danmaku_alpha(lv_event_t*) {
    setting_danmaku_alpha = setting_danmaku_alpha >= 100 ? 10 : setting_danmaku_alpha + 10;
    save_lvgl_config();
    if (player_page_visible && active_video_index < displayed_videos.size()) show_player_page(active_video_index);
    else if (main_section == MainSection::Settings) {
        if (board_preview) show_main_section(MainSection::Settings);
        else show_settings_page();
    }
}

void add_quality_button(lv_obj_t* parent, const char* title, int* value, uintptr_t kind, int y) {
    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_size(button, 730, 54);
    lv_obj_set_pos(button, 28, y);
    lv_obj_set_style_bg_color(button, ui_color(0x202735), 0);
    lv_obj_set_style_radius(button, 5, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_add_event_cb(button, cycle_quality_setting, LV_EVENT_CLICKED, reinterpret_cast<void*>(kind));
    const std::string label = std::string(title) + "   " + std::to_string(*value);
    set_label(button, label.c_str(), 15, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 16, 0);
}

void cycle_playback_speed(lv_event_t*) {
    static const int options[] = {50, 75, 100, 125, 150, 200};
    auto it = std::find(std::begin(options), std::end(options), setting_playback_speed);
    const size_t index = it == std::end(options) ? 2 : static_cast<size_t>(it - std::begin(options));
    setting_playback_speed = options[(index + 1) % (sizeof(options) / sizeof(options[0]))];
    save_lvgl_config();
    std::string error;
    if (!platform_player_set_speed(static_cast<uint16_t>(setting_playback_speed), error)) {
        player_backend_error = error;
        if (status_label) lv_label_set_text(status_label, player_backend_error.c_str());
        return;
    }
    if (player_page_visible && active_video_index < displayed_videos.size()) show_player_page(active_video_index);
}

void cycle_player_strategy(lv_event_t*) {
    setting_player_strategy = (setting_player_strategy + 1) % 4;
    save_lvgl_config();
    if (main_section == MainSection::Settings) {
        if (board_preview) show_main_section(MainSection::Settings);
        else show_settings_page();
    }
}

void handle_player_end() {
    if (!player_page_visible || !player_playing || !platform_player_finished()) return;
    player_playing = false;
    platform_player_stop();
    if (active_video_index >= displayed_videos.size()) return;
    if (setting_player_strategy == 2) {
        show_player_page(active_video_index);
        return;
    }
    if (setting_player_strategy == 3) {
        if (status_label) lv_label_set_text(status_label, "Playback ended");
        show_player_page(active_video_index);
        return;
    }
    const Video& video = displayed_videos[active_video_index];
    if (active_part_index + 1 < video.parts.size()) {
        request_video_detail(active_video_index, active_part_index + 1);
        open_player_after_detail = true;
        return;
    }
    if (setting_player_strategy == 0 && !video.related.empty()) {
        displayed_videos.clear();
        Video next;
        next.bvid = video.related.front().bvid;
        next.title = video.related.front().title;
        next.owner = video.related.front().owner;
        displayed_videos.push_back(std::move(next));
        active_video_index = 0;
        request_video_detail(0);
        open_player_after_detail = true;
        return;
    }
    if (active_video_index + 1 < displayed_videos.size()) {
        ++active_video_index;
        request_video_detail(active_video_index);
        open_player_after_detail = true;
    } else if (status_label) {
        lv_label_set_text(status_label, "Playback ended: no next item");
    }
}

void cycle_player_aspect(lv_event_t*) {
    static const char* options[] = {"default", "4:3", "16:9"};
    size_t index = 0;
    for (size_t i = 0; i < 3; ++i) if (setting_aspect == options[i]) index = i;
    setting_aspect = options[(index + 1) % 3];
    save_lvgl_config();
    if (player_page_visible && active_video_index < displayed_videos.size()) show_player_page(active_video_index);
}

void add_setting_switch(lv_obj_t* parent, const char* title, const char* detail, bool* value, int y) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, 730, 64);
    lv_obj_set_pos(row, 28, y);
    lv_obj_set_style_bg_color(row, ui_color(0x202735), 0);
    lv_obj_set_style_radius(row, 5, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lock_static_canvas(row);
    set_label(row, title, 17, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 16, -11);
    set_label(row, detail, 13, ui_color(0xaeb8c6), LV_ALIGN_LEFT_MID, 16, 15);
    lv_obj_t* state = lv_obj_create(row);
    lv_obj_set_size(state, 58, 26);
    lv_obj_align(state, LV_ALIGN_RIGHT_MID, -16, 0);
    lv_obj_set_style_bg_color(state, ui_color(*value ? 0x0ea5e9 : 0x344256), 0);
    lv_obj_set_style_radius(state, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(state, 0, 0);
    lock_static_canvas(state);
    set_label(state, *value ? "ON" : "OFF", 11, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, toggle_setting_button, LV_EVENT_CLICKED, value);
}

void show_settings_page() {
    if (content) lv_obj_del(content);
    proxy_input = nullptr;
    detail_visible = false;
    content = lv_obj_create(lv_scr_act());
    lv_obj_set_size(content, 1120, 590);
    lv_obj_set_pos(content, 130, 110);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    const char* categories[] = {"Playback", "Interface", "Other"};
    lv_obj_t* nav = lv_obj_create(content);
    lv_obj_set_size(nav, 260, 570);
    lv_obj_set_pos(nav, 0, 0);
    lv_obj_set_style_bg_color(nav, ui_color(0x1d2330), 0);
    lv_obj_set_style_radius(nav, 6, 0);
    lv_obj_set_style_border_width(nav, 0, 0);
    // This category rail has fixed content. Leaving it scrollable makes LVGL
    // draw an idle horizontal scrollbar when it receives a mouse-wheel event.
    lv_obj_clear_flag(nav, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(nav, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(nav, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_set_scrollbar_mode(nav, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(nav, LV_DIR_NONE);
    lv_obj_set_style_bg_opa(nav, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(nav, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_outline_width(nav, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_opa(nav, LV_OPA_TRANSP, LV_STATE_FOCUSED);
    set_label(nav, "Settings", 20, ui_color(0xf3f5f8), LV_ALIGN_TOP_LEFT, 20, 18);
    for (unsigned int i = 0; i < 3; ++i) {
        lv_obj_t* button = lv_btn_create(nav);
        lv_obj_set_size(button, 220, 46);
        lv_obj_set_pos(button, 20, 62 + static_cast<int>(i) * 56);
        lv_obj_set_style_bg_color(button, ui_color(i == settings_active_tab ? 0x2877a9 : 0x374152), 0);
        lv_obj_set_style_radius(button, 5, 0);
        lv_obj_set_style_border_width(button, 0, 0);
        lv_obj_add_event_cb(button, select_settings_tab, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
        set_label(button, categories[i], 16, ui_color(0xffffff), LV_ALIGN_LEFT_MID, 16, 0);
    }
    lv_obj_t* pane = lv_obj_create(content);
    lv_obj_set_size(pane, 830, 570);
    lv_obj_set_pos(pane, 290, 0);
    lv_obj_set_style_bg_color(pane, ui_color(0x1d2330), 0);
    lv_obj_set_style_radius(pane, 6, 0);
    lv_obj_set_style_border_width(pane, 0, 0);
    lv_obj_add_flag(pane, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(pane, LV_SCROLLBAR_MODE_AUTO);
    set_label(pane, categories[settings_active_tab], 21, ui_color(0xf3f5f8), LV_ALIGN_TOP_LEFT, 28, 20);
    if (settings_active_tab == 0) {
        add_setting_switch(pane, "Hardware decoding", "Use the configured hardware video decoder", &setting_hwdec, 66);
        add_setting_switch(pane, "Auto fullscreen", "Enter fullscreen when playback starts", &setting_autofullscreen, 140);
        add_setting_switch(pane, "Low quality mode", "Prefer a lower stream quality to reduce bandwidth", &setting_low_quality, 214);
        add_quality_button(pane, "Video quality (qn)", &setting_video_quality, 1, 288);
        add_quality_button(pane, "Audio quality", &setting_audio_quality, 2, 350);
        add_quality_button(pane, "Memory cache (MiB)", &setting_memory_cache, 3, 412);
        add_video_option_button(pane, "Video format", setting_video_format == 4048 ? "DASH (AVC/HEVC/AV1)" : "FLV/MP4",
                                cycle_video_format, 486);
        const char* codec = setting_video_codec == 12 ? "HEVC/H.265" : setting_video_codec == 13 ? "AV1" : "AVC/H.264";
        add_video_option_button(pane, "Video codec", codec, cycle_video_codec, 548);
        lv_obj_t* aspect = lv_btn_create(pane);
        lv_obj_set_size(aspect, 730, 54);
        lv_obj_set_pos(aspect, 28, 610);
        lv_obj_set_style_bg_color(aspect, ui_color(0x202735), 0);
        lv_obj_set_style_radius(aspect, 5, 0);
        lv_obj_set_style_border_width(aspect, 0, 0);
        lv_obj_add_event_cb(aspect, cycle_player_aspect, LV_EVENT_CLICKED, nullptr);
        const std::string aspect_text = "Video aspect   " + setting_aspect;
        set_label(aspect, aspect_text.c_str(), 15, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 16, 0);
        static const char* strategies[] = {"Recommended", "Next", "Loop", "Single"};
        lv_obj_t* strategy = lv_btn_create(pane);
        lv_obj_set_size(strategy, 730, 54);
        lv_obj_set_pos(strategy, 28, 672);
        lv_obj_set_style_bg_color(strategy, ui_color(0x202735), 0);
        lv_obj_set_style_radius(strategy, 5, 0);
        lv_obj_set_style_border_width(strategy, 0, 0);
        lv_obj_add_event_cb(strategy, cycle_player_strategy, LV_EVENT_CLICKED, nullptr);
        const std::string strategy_text = std::string("End of playback   ") + strategies[setting_player_strategy];
        set_label(strategy, strategy_text.c_str(), 15, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 16, 0);
        add_setting_switch(pane, "Watch history reporting", "Report playback progress to the signed-in Bilibili account", &setting_history_report, 734);
    } else if (settings_active_tab == 1) {
        lv_obj_t* scale = lv_btn_create(pane);
        lv_obj_set_size(scale, 730, 58);
        lv_obj_set_pos(scale, 28, 66);
        lv_obj_set_style_bg_color(scale, ui_color(0x202735), 0);
        lv_obj_set_style_radius(scale, 5, 0);
        lv_obj_set_style_border_width(scale, 0, 0);
        lv_obj_add_event_cb(scale, cycle_ui_scale, LV_EVENT_CLICKED, nullptr);
        const std::string scale_text = "Display scale   " + std::to_string(setting_ui_scale) + "%";
        set_label(scale, scale_text.c_str(), 17, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 16, 0);
        set_label(pane, "Window, theme, bottom bar, FPS, keymap, TV OSD,\n"
                        "language, and UI style remain source-compatible settings.",
                  14, ui_color(0xaeb8c6), LV_ALIGN_TOP_LEFT, 30, 150);
        add_setting_switch(pane, "Show FPS", "Display the LVGL frame-rate indicator", &setting_show_fps, 210);
        add_setting_switch(pane, "Hide bottom bar", "Remove the navigation bar while browsing", &setting_hide_bottom_bar, 284);
        add_setting_switch(pane, "TV OSD mode", "Use compact controls for live-TV playback", &setting_tv_osd, 358);
        lv_obj_t* fps_limit = lv_btn_create(pane);
        lv_obj_set_size(fps_limit, 730, 54);
        lv_obj_set_pos(fps_limit, 28, 432);
        lv_obj_set_style_bg_color(fps_limit, ui_color(0x202735), 0);
        lv_obj_set_style_radius(fps_limit, 5, 0);
        lv_obj_set_style_border_width(fps_limit, 0, 0);
        lv_obj_add_event_cb(fps_limit, cycle_limited_fps, LV_EVENT_CLICKED, nullptr);
        const std::string fps_text = "Frame-rate limit   " + (setting_limited_fps == 0 ? std::string("Unlimited") : std::to_string(setting_limited_fps) + " FPS");
        set_label(fps_limit, fps_text.c_str(), 15, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 16, 0);
        add_setting_switch(pane, "Swap A/B", "Swap gamepad confirm and back buttons", &setting_swap_abxy, 494);
        lv_obj_t* keymap = lv_btn_create(pane);
        lv_obj_set_size(keymap, 730, 54);
        lv_obj_set_pos(keymap, 28, 568);
        lv_obj_set_style_bg_color(keymap, ui_color(0x202735), 0);
        lv_obj_set_style_radius(keymap, 5, 0);
        lv_obj_set_style_border_width(keymap, 0, 0);
        lv_obj_add_event_cb(keymap, cycle_keymap, LV_EVENT_CLICKED, nullptr);
        const std::string keymap_text = "Button glyph set   " + setting_keymap;
        set_label(keymap, keymap_text.c_str(), 15, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 16, 0);
        lv_obj_t* theme = lv_btn_create(pane);
        lv_obj_set_size(theme, 730, 54);
        lv_obj_set_pos(theme, 28, 630);
        lv_obj_set_style_bg_color(theme, ui_color(0x202735), 0);
        lv_obj_set_style_radius(theme, 5, 0);
        lv_obj_set_style_border_width(theme, 0, 0);
        lv_obj_add_event_cb(theme, cycle_app_theme, LV_EVENT_CLICKED, nullptr);
        const std::string theme_text = "Color theme   " + setting_app_theme;
        set_label(theme, theme_text.c_str(), 15, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 16, 0);
    } else {
        add_setting_switch(pane, "TLS certificate verification", "Verify HTTPS certificates for Bilibili requests", &setting_tls_verify, 66);
        add_setting_switch(pane, "Danmaku", "Show timed scrolling comments over video", &setting_danmaku_on, 140);
        lv_obj_t* danmaku_speed = lv_btn_create(pane);
        lv_obj_set_size(danmaku_speed, 730, 54);
        lv_obj_set_pos(danmaku_speed, 28, 214);
        lv_obj_set_style_bg_color(danmaku_speed, ui_color(0x202735), 0);
        lv_obj_set_style_border_width(danmaku_speed, 0, 0);
        lv_obj_add_event_cb(danmaku_speed, cycle_danmaku_speed, LV_EVENT_CLICKED, nullptr);
        const std::string danmaku_speed_text = "Danmaku speed (%)   " + std::to_string(setting_danmaku_speed);
        set_label(danmaku_speed, danmaku_speed_text.c_str(), 15, ui_color(0xffffff), LV_ALIGN_LEFT_MID, 16, 0);
        add_setting_switch(pane, "HTTP proxy", "Route Bilibili HTTPS requests through the saved proxy", &setting_http_proxy_enabled, 288);
        set_label(pane, "Proxy address (for example 127.0.0.1:7890)", 14, ui_color(0xaeb8c6),
                  LV_ALIGN_TOP_LEFT, 30, 366);
        proxy_input = lv_textarea_create(pane);
        lv_obj_set_size(proxy_input, 550, 46);
        lv_obj_set_pos(proxy_input, 28, 394);
        lv_textarea_set_one_line(proxy_input, true);
        lv_textarea_set_placeholder_text(proxy_input, "host:port");
        lv_textarea_set_text(proxy_input, setting_http_proxy.c_str());
        style_dark_textarea(proxy_input);
        lv_obj_add_event_cb(proxy_input, focus_http_proxy, LV_EVENT_FOCUSED, nullptr);
        lv_obj_add_event_cb(proxy_input, unfocus_http_proxy, LV_EVENT_DEFOCUSED, nullptr);
        lv_obj_t* proxy_save = lv_btn_create(pane);
        lv_obj_set_size(proxy_save, 160, 46);
        lv_obj_set_pos(proxy_save, 598, 394);
        lv_obj_set_style_bg_color(proxy_save, ui_color(0x2877a9), 0);
        lv_obj_set_style_border_width(proxy_save, 0, 0);
        lv_obj_add_event_cb(proxy_save, save_http_proxy, LV_EVENT_CLICKED, nullptr);
        set_label(proxy_save, "Save proxy", 14, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        lv_obj_t* image_threads = lv_btn_create(pane);
        lv_obj_set_size(image_threads, 730, 54);
        lv_obj_set_pos(image_threads, 28, 456);
        lv_obj_set_style_bg_color(image_threads, ui_color(0x202735), 0);
        lv_obj_set_style_border_width(image_threads, 0, 0);
        lv_obj_add_event_cb(image_threads, cycle_image_request_threads, LV_EVENT_CLICKED, nullptr);
        const std::string image_threads_text = "Image request workers   " + std::to_string(setting_image_threads);
        set_label(image_threads, image_threads_text.c_str(), 15, ui_color(0xffffff), LV_ALIGN_LEFT_MID, 16, 0);
        lv_obj_t* connect_timeout = lv_btn_create(pane);
        lv_obj_set_size(connect_timeout, 730, 54);
        lv_obj_set_pos(connect_timeout, 28, 518);
        lv_obj_set_style_bg_color(connect_timeout, ui_color(0x202735), 0);
        lv_obj_set_style_border_width(connect_timeout, 0, 0);
        lv_obj_add_event_cb(connect_timeout, cycle_http_timeout, LV_EVENT_CLICKED, reinterpret_cast<void*>(1));
        const std::string connect_timeout_text = "Connection timeout   " + std::to_string(setting_http_connect_timeout_seconds) + " sec";
        set_label(connect_timeout, connect_timeout_text.c_str(), 15, ui_color(0xffffff), LV_ALIGN_LEFT_MID, 16, 0);
        lv_obj_t* response_timeout = lv_btn_create(pane);
        lv_obj_set_size(response_timeout, 730, 54);
        lv_obj_set_pos(response_timeout, 28, 580);
        lv_obj_set_style_bg_color(response_timeout, ui_color(0x202735), 0);
        lv_obj_set_style_border_width(response_timeout, 0, 0);
        lv_obj_add_event_cb(response_timeout, cycle_http_timeout, LV_EVENT_CLICKED, nullptr);
        const std::string response_timeout_text = "Response timeout   " + std::to_string(setting_http_timeout_seconds) + " sec";
        set_label(response_timeout, response_timeout_text.c_str(), 15, ui_color(0xffffff), LV_ALIGN_LEFT_MID, 16, 0);
        lv_obj_t* danmaku_area = lv_btn_create(pane);
        lv_obj_set_size(danmaku_area, 730, 54);
        lv_obj_set_pos(danmaku_area, 28, 642);
        lv_obj_set_style_bg_color(danmaku_area, ui_color(0x202735), 0);
        lv_obj_set_style_border_width(danmaku_area, 0, 0);
        lv_obj_add_event_cb(danmaku_area, cycle_danmaku_area, LV_EVENT_CLICKED, nullptr);
        const std::string danmaku_area_text = "Danmaku display area   " + std::to_string(setting_danmaku_area) + "%";
        set_label(danmaku_area, danmaku_area_text.c_str(), 15, ui_color(0xffffff), LV_ALIGN_LEFT_MID, 16, 0);
        lv_obj_t* danmaku_alpha = lv_btn_create(pane);
        lv_obj_set_size(danmaku_alpha, 730, 54);
        lv_obj_set_pos(danmaku_alpha, 28, 704);
        lv_obj_set_style_bg_color(danmaku_alpha, ui_color(0x202735), 0);
        lv_obj_set_style_border_width(danmaku_alpha, 0, 0);
        lv_obj_add_event_cb(danmaku_alpha, cycle_danmaku_alpha, LV_EVENT_CLICKED, nullptr);
        const std::string danmaku_alpha_text = "Danmaku opacity   " + std::to_string(setting_danmaku_alpha) + "%";
        set_label(danmaku_alpha, danmaku_alpha_text.c_str(), 15, ui_color(0xffffff), LV_ALIGN_LEFT_MID, 16, 0);
    }
    lv_label_set_text(status_label, "Settings: persistent LVGL compatibility subset");
}

void select_inbox_tab(lv_event_t* event) {
    inbox_active_tab = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
    if (inbox_active_tab < inbox_fetch_requested.size()) inbox_fetch_requested[inbox_active_tab] = false;
    active_chat_talker.clear();
    displayed_chat_messages.clear();
    if (board_preview) show_main_section(MainSection::Inbox);
    else show_inbox_page();
}

void show_inbox_page() {
    if (active_text_input == inbox_message_input) active_text_input = nullptr;
    inbox_message_input = nullptr;
    if (content) lv_obj_del(content);
    detail_visible = false;
    content = lv_obj_create(lv_scr_act());
    lv_obj_set_size(content, 1120, 590);
    lv_obj_set_pos(content, 130, 110);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    const char* tabs[] = {"Chat", "Replies", "@ mentions", "Likes"};

    // A message drawer is useful once conversations exist.  Showing that
    // drawer before login leaves a dead, dark half-page on the left.
    if (account_name.empty()) {
        lv_obj_t* panel = lv_obj_create(content);
        lv_obj_set_size(panel, 880, 500);
        lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(panel, ui_color(0x1d2330), 0);
        lv_obj_set_style_radius(panel, 8, 0);
        lv_obj_set_style_border_width(panel, 0, 0);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
        for (unsigned int i = 0; i < 4; ++i) {
            lv_obj_t* tab = lv_btn_create(panel);
            lv_obj_set_size(tab, 158, 42);
            lv_obj_set_pos(tab, 106 + static_cast<int>(i) * 166, 24);
            lv_obj_set_style_bg_color(tab, ui_color(i == inbox_active_tab ? 0x2877a9 : 0x374152), 0);
            lv_obj_set_style_radius(tab, 5, 0);
            lv_obj_set_style_border_width(tab, 0, 0);
            lv_obj_add_event_cb(tab, select_inbox_tab, LV_EVENT_CLICKED,
                                reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
            set_label(tab, tabs[i], 15, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        }
        lv_obj_t* badge = lv_obj_create(panel);
        lv_obj_set_size(badge, 62, 62);
        lv_obj_align(badge, LV_ALIGN_CENTER, 0, -64);
        lv_obj_set_style_bg_color(badge, ui_color(0x203d55), 0);
        lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(badge, 0, 0);
        lock_static_canvas(badge);
        set_symbol(badge, LV_SYMBOL_BELL, ui_color(0x63c4f4));
        set_label(panel, tabs[inbox_active_tab], 22, ui_color(0xf3f5f8), LV_ALIGN_CENTER, 0, 12);
        set_label(panel, "Sign in with a Bilibili Cookie to load messages.",
                  16, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 46);
        lv_label_set_text(status_label, "Inbox: login required");
        return;
    }
    // inbox_view.xml is a 600px right-hand applet over a backdrop. Keep that
    // shape instead of making it a full-page feed.
    lv_obj_t* backdrop = lv_obj_create(content);
    lv_obj_set_size(backdrop, 1120, 590);
    lv_obj_set_style_bg_color(backdrop, ui_color(0x080b10), 0);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_50, 0);
    lv_obj_set_style_border_width(backdrop, 0, 0);
    lv_obj_t* drawer = lv_obj_create(content);
    lv_obj_set_size(drawer, 650, 590);
    lv_obj_align(drawer, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(drawer, ui_color(0x1d2330), 0);
    lv_obj_set_style_border_width(drawer, 0, 0);
    lv_obj_set_style_radius(drawer, 6, 0);
    for (unsigned int i = 0; i < 4; ++i) {
        lv_obj_t* tab = lv_btn_create(drawer);
        lv_obj_set_size(tab, 120, 42);
        lv_obj_set_pos(tab, 20 + static_cast<int>(i) * 128, 18);
        lv_obj_set_style_bg_color(tab, ui_color(i == inbox_active_tab ? 0x2877a9 : 0x374152), 0);
        lv_obj_set_style_radius(tab, 5, 0);
        lv_obj_set_style_border_width(tab, 0, 0);
        lv_obj_add_event_cb(tab, select_inbox_tab, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
        set_label(tab, tabs[i], 15, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    }
    lv_obj_t* close = lv_btn_create(drawer);
    lv_obj_set_size(close, 40, 40);
    lv_obj_align(close, LV_ALIGN_TOP_RIGHT, -12, 18);
    lv_obj_set_style_bg_color(close, ui_color(0x374152), 0);
    lv_obj_set_style_radius(close, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(close, 0, 0);
    lv_obj_add_event_cb(close, select_main_section, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(MainSection::Home)));
    // The configured FreeType font does not provide LVGL's private-use close
    // glyph. Rendering it can stall LVGL's software glyph lookup on Windows.
    set_label(close, "X", 18, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    lv_obj_t* empty = lv_obj_create(drawer);
    lv_obj_set_size(empty, 610, 480);
    lv_obj_set_pos(empty, 20, 82);
    lv_obj_set_style_bg_color(empty, ui_color(0x202735), 0);
    lv_obj_set_style_border_width(empty, 0, 0);
    lv_obj_set_style_radius(empty, 5, 0);
    inbox_message_input = nullptr;
    if (account_name.empty()) {
        set_label(empty, tabs[inbox_active_tab], 20, ui_color(0xf3f5f8), LV_ALIGN_CENTER, 0, -28);
        set_label(empty, "Sign in with a Bilibili Cookie to load messages.",
                  16, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 28);
        lv_label_set_text(status_label, "Inbox: login required");
    } else if (displayed_inbox_items.empty()) {
        const bool unavailable = inbox_active_tab < inbox_fetch_requested.size() &&
            inbox_fetch_requested[inbox_active_tab] && !inbox_fetch_in_flight;
        set_label(empty, unavailable ? "No messages or unavailable" : "Loading messages...",
                  20, ui_color(0xf3f5f8), LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(status_label, unavailable ? "Inbox: select tab to retry" : "Inbox: loading authenticated feed");
        if (inbox_active_tab < inbox_fetch_requested.size() && !inbox_fetch_requested[inbox_active_tab])
            fetch_inbox_items(inbox_active_tab);
    } else {
        const bool showing_thread = inbox_active_tab == 0 && !active_chat_talker.empty() && !displayed_chat_messages.empty();
        const std::vector<InboxItem>& items = showing_thread ? displayed_chat_messages : displayed_inbox_items;
        lv_label_set_text(status_label, showing_thread ? "Inbox: chat thread" : "Inbox: authenticated feed");
        if (showing_thread) {
            lv_obj_t* back = lv_btn_create(empty);
            lv_obj_set_size(back, 82, 32);
            lv_obj_set_pos(back, 18, 10);
            lv_obj_add_event_cb(back, close_chat_thread, LV_EVENT_CLICKED, nullptr);
            set_label(back, "Back", 13, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
            lv_obj_t* more = lv_btn_create(empty);
            lv_obj_set_size(more, 118, 32);
            lv_obj_set_pos(more, 110, 10);
            lv_obj_set_style_bg_color(more, ui_color(0x374152), 0);
            lv_obj_set_style_border_width(more, 0, 0);
            lv_obj_add_event_cb(more, load_more_chat_messages, LV_EVENT_CLICKED, nullptr);
            set_label(more, "More messages", 12, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
            inbox_message_input = lv_textarea_create(empty);
            lv_obj_set_size(inbox_message_input, 410, 36);
            lv_obj_set_pos(inbox_message_input, 18, 432);
            lv_textarea_set_one_line(inbox_message_input, true);
            lv_textarea_set_placeholder_text(inbox_message_input, "Write a message");
            style_dark_textarea(inbox_message_input);
            lv_obj_add_event_cb(inbox_message_input, focus_inbox_message, LV_EVENT_FOCUSED, nullptr);
            lv_obj_add_event_cb(inbox_message_input, unfocus_inbox_message, LV_EVENT_DEFOCUSED, nullptr);
            lv_obj_t* send = lv_btn_create(empty);
            lv_obj_set_size(send, 126, 36);
            lv_obj_set_pos(send, 454, 432);
            lv_obj_add_event_cb(send, send_chat_message, LV_EVENT_CLICKED, nullptr);
            set_label(send, "Send", 13, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        }
        for (size_t index = 0; index < items.size(); ++index) {
            const InboxItem& item = items[index];
            lv_obj_t* row = lv_obj_create(empty);
            lv_obj_set_size(row, 570, 58);
            lv_obj_set_pos(row, 18, (showing_thread ? 54 : 14) + static_cast<int>(index) * 46);
            lv_obj_set_style_bg_color(row, ui_color(0x1d2330), 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_radius(row, 4, 0);
            if (!showing_thread && inbox_active_tab == 0 && !item.talker_id.empty()) {
                lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_add_event_cb(row, select_chat_session, LV_EVENT_CLICKED,
                                    reinterpret_cast<void*>(static_cast<uintptr_t>(index)));
            }
            set_label(row, item.sender.c_str(), 14, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 10, -10);
            const std::string meta = (item.timestamp.empty() ? std::string{} : "t=" + item.timestamp) +
                                     (item.unread_count.empty() || item.unread_count == "0" ? std::string{} : "  unread " + item.unread_count);
            if (!meta.empty()) set_label(row, meta.c_str(), 10, ui_color(0x7e8a9a), LV_ALIGN_RIGHT_MID, -10, -10);
            set_label(row, item.message.c_str(), 12, ui_color(0xaeb8c6), LV_ALIGN_LEFT_MID, 10, 13);
        }
    }
}

lv_obj_t* board_tab_button(lv_obj_t* parent, const char* label, int x, int width, bool active,
                           lv_event_cb_t callback, uintptr_t value) {
    lv_obj_t* tab = lv_btn_create(parent);
    lv_obj_set_size(tab, width, 34);
    lv_obj_set_pos(tab, x, 0);
    lv_obj_set_style_bg_color(tab, ui_color(active ? 0x0ea5e9 : 0x1f2937), 0);
    lv_obj_set_style_radius(tab, 7, 0);
    lv_obj_set_style_border_width(tab, 0, 0);
    lv_obj_add_event_cb(tab, callback, LV_EVENT_CLICKED, reinterpret_cast<void*>(value));
    set_label(tab, label, 13, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    return tab;
}

void board_setting_switch(lv_obj_t* parent, const char* title, const char* detail, bool* value, int y) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, 720, 68);
    lv_obj_set_pos(row, 0, y);
    lv_obj_set_style_bg_color(row, ui_color(0x182231), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 7, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lock_static_canvas(row);
    set_label(row, title, 15, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 16, -11);
    set_label(row, detail, 12, ui_color(0xaeb8c6), LV_ALIGN_LEFT_MID, 16, 14);
    lv_obj_t* state = lv_obj_create(row);
    lv_obj_set_size(state, 58, 26);
    lv_obj_align(state, LV_ALIGN_RIGHT_MID, -16, 0);
    lv_obj_set_style_bg_color(state, ui_color(*value ? 0x0ea5e9 : 0x29384b), 0);
    lv_obj_set_style_radius(state, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(state, 0, 0);
    lv_obj_set_style_pad_all(state, 0, 0);
    lock_static_canvas(state);
    set_label(state, *value ? "ON" : "OFF", 11, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, toggle_setting_button, LV_EVENT_CLICKED, value);
}

void board_empty_state(lv_obj_t* parent, const char* symbol, const char* title, const char* detail) {
    lv_obj_t* badge = lv_obj_create(parent);
    lv_obj_set_size(badge, 54, 54);
    lv_obj_align(badge, LV_ALIGN_CENTER, 0, -68);
    lv_obj_set_style_bg_color(badge, ui_color(0x203d55), 0);
    lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lock_static_canvas(badge);
    set_symbol(badge, symbol, ui_color(0x63c4f4));
    set_label(parent, title, 18, ui_color(0xf3f5f8), LV_ALIGN_CENTER, 0, 0);
    set_label(parent, detail, 13, ui_color(0x9dacbd), LV_ALIGN_CENTER, 0, 30);
}

void show_board_section(MainSection section) {
    if (content) lv_obj_del(content);
    detail_visible = false;
    content = lv_obj_create(lv_scr_act());
    lv_obj_set_size(content, 720, 382);
    lv_obj_set_pos(content, 72, 100);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lock_static_canvas(content);

    if (section == MainSection::Dynamic) {
        lv_label_set_text(status_label, account_name.empty() ? "Dynamic / sign in required" : "Dynamic / authenticated");
        board_tab_button(content, "Videos", 0, 106, !dynamic_showing_articles, select_dynamic_tab, 0);
        board_tab_button(content, "Articles", 116, 106, dynamic_showing_articles, select_dynamic_tab, 1);
        board_tab_button(content, "More", 232, 106, false, load_more_dynamic, 0);
        lv_obj_t* pane = lv_obj_create(content);
        lv_obj_set_size(pane, 720, 332);
        lv_obj_set_pos(pane, 0, 50);
        lv_obj_set_style_bg_color(pane, ui_color(0x1d2330), 0);
        lv_obj_set_style_border_width(pane, 0, 0);
        lv_obj_set_style_radius(pane, 4, 0);
        lv_obj_set_scroll_dir(pane, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(pane, LV_SCROLLBAR_MODE_AUTO);
        if (account_name.empty()) {
            board_empty_state(pane, LV_SYMBOL_LIST,
                              dynamic_showing_articles ? "Following articles" : "Following videos",
                              "Sign in to see the creators you follow");
        } else if (displayed_dynamic_items.empty()) {
            set_label(pane, dynamic_fetch_requested && !dynamic_fetch_in_flight ?
                      "No updates or unavailable" : "Loading dynamic feed...",
                      18, ui_color(0xf3f5f8), LV_ALIGN_CENTER, 0, 0);
            if (!dynamic_fetch_requested) fetch_dynamic_items(dynamic_showing_articles);
        } else {
            for (size_t index = 0; index < displayed_dynamic_items.size() && index < 4; ++index) {
                const DynamicItem& item = displayed_dynamic_items[index];
                lv_obj_t* row = item.bvid.empty() && item.id.empty() ? lv_obj_create(pane) : lv_btn_create(pane);
                lv_obj_set_size(row, 690, 62);
                lv_obj_set_pos(row, 14, 8 + static_cast<int>(index) * 70);
                lv_obj_set_style_bg_color(row, ui_color(0x202735), 0);
                lv_obj_set_style_border_width(row, 0, 0);
                lv_obj_set_style_radius(row, 4, 0);
                if (!item.bvid.empty() || !item.id.empty())
                    lv_obj_add_event_cb(row, open_dynamic_item, LV_EVENT_CLICKED,
                                        reinterpret_cast<void*>(static_cast<uintptr_t>(index + 1)));
                dynamic_row_line(row, item.title, 14, ui_color(0xf3f5f8), 610, 7);
                if (!item.summary.empty())
                    dynamic_row_line(row, item.summary, 11, ui_color(0xaeb8c6), 610, 35);
                if (!item.id.empty()) {
                    lv_obj_t* like = lv_btn_create(row);
                    lv_obj_set_size(like, 48, 24);
                    lv_obj_align(like, LV_ALIGN_RIGHT_MID, -8, 0);
                    lv_obj_add_event_cb(like, like_dynamic_item, LV_EVENT_CLICKED,
                                        reinterpret_cast<void*>(static_cast<uintptr_t>(index)));
                    set_label(like, "Like", 10, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
                }
            }
        }
        return;
    }

    if (section == MainSection::Mine) {
        lv_label_set_text(status_label, account_name.empty() ? "Mine / sign in required" : "Mine / authenticated");
        lv_obj_t* user = lv_obj_create(content);
        lv_obj_set_size(user, 720, 88);
        lv_obj_set_style_bg_color(user, ui_color(0x1d2330), 0);
        lv_obj_set_style_border_width(user, 0, 0);
        lv_obj_set_style_radius(user, 4, 0);
        lock_static_canvas(user);
        set_label(user, "@", 24, ui_color(0xdce8f3), LV_ALIGN_LEFT_MID, 22, 0);
        set_label(user, account_name.empty() ? "Click to sign in" : account_name.c_str(), 17, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 64, -12);
        set_label(user, account_name.empty() ? "Import a Cookie session to sign in" : "Cookie session is active", 13, ui_color(0xaeb8c6),
                  LV_ALIGN_LEFT_MID, 64, 14);
        if (!account_name.empty()) {
            lv_obj_t* logout = lv_btn_create(user);
            lv_obj_set_size(logout, 82, 32);
            lv_obj_align(logout, LV_ALIGN_RIGHT_MID, -12, 0);
            lv_obj_add_event_cb(logout, logout_account, LV_EVENT_CLICKED, nullptr);
            set_label(logout, "Sign out", 11, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        }
        const char* tabs[] = {"History", "Favorites", "Sub", "Anime", "Series", "Later"};
        for (unsigned int i = 0; i < 6; ++i) {
            lv_obj_t* tab = board_tab_button(content, tabs[i], static_cast<int>(i) * 120, 112,
                                             i == mine_active_tab, select_mine_tab, i);
            lv_obj_set_y(tab, 102);
        }
        lv_obj_t* pane = lv_obj_create(content);
        lv_obj_set_size(pane, 720, 198);
        lv_obj_set_pos(pane, 0, 184);
        lv_obj_set_style_bg_color(pane, ui_color(0x1d2330), 0);
        lv_obj_set_style_border_width(pane, 0, 0);
        lv_obj_set_style_radius(pane, 4, 0);
        lock_static_canvas(pane);
        set_label(pane, tabs[mine_active_tab], 18, ui_color(0xf3f5f8), LV_ALIGN_TOP_LEFT, 16, 14);
        if (account_name.empty()) {
            set_label(pane, "Sign in to load this source-defined page.", 14, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 20);
        } else if (mine_active_tab == 0 && displayed_history_videos.empty()) {
            set_label(pane, mine_fetch_requested[0] && !history_fetch_in_flight ? "No history or unavailable" :
                      "Loading watch history...", 14, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 20);
            if (!mine_fetch_requested[0]) fetch_history_videos();
        } else if (mine_active_tab == 0) {
            for (size_t index = 0; index < displayed_history_videos.size() && index < 3; ++index) {
                const Video& item = displayed_history_videos[index];
                lv_obj_t* row = lv_obj_create(pane);
                lv_obj_set_size(row, 680, 46);
                lv_obj_set_pos(row, 16, 48 + static_cast<int>(index) * 50);
                lv_obj_set_style_bg_color(row, ui_color(0x202735), 0);
                lv_obj_set_style_border_width(row, 0, 0);
                lv_obj_set_style_radius(row, 4, 0);
                set_label(row, item.title.c_str(), 13, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 10, 0);
                lv_obj_t* remove = lv_btn_create(row);
                lv_obj_set_size(remove, 64, 28);
                lv_obj_align(remove, LV_ALIGN_RIGHT_MID, -8, 0);
                lv_obj_set_style_bg_color(remove, ui_color(0x8d3b45), 0);
                lv_obj_set_style_border_width(remove, 0, 0);
                lv_obj_add_event_cb(remove, remove_favorite_video, LV_EVENT_CLICKED,
                                    reinterpret_cast<void*>(static_cast<uintptr_t>(index)));
                set_label(remove, "Remove", 10, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
            }
        } else if (mine_active_tab == 1 && displayed_favorite_videos.empty()) {
            set_label(pane, mine_fetch_requested[1] && !favorite_fetch_in_flight ? "No favorites or unavailable" :
                      "Loading favorites...", 14, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 20);
            if (!mine_fetch_requested[1]) fetch_favorite_videos();
        } else if (mine_active_tab == 1) {
            for (size_t index = 0; index < displayed_favorite_videos.size() && index < 3; ++index) {
                const Video& item = displayed_favorite_videos[index];
                lv_obj_t* row = lv_obj_create(pane);
                lv_obj_set_size(row, 680, 46);
                lv_obj_set_pos(row, 16, 48 + static_cast<int>(index) * 50);
                lv_obj_set_style_bg_color(row, ui_color(0x202735), 0);
                lv_obj_set_style_border_width(row, 0, 0);
                lv_obj_set_style_radius(row, 4, 0);
                set_label(row, item.title.c_str(), 13, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 10, 0);
            }
        } else if (mine_active_tab == 5 && displayed_later_videos.empty()) {
            set_label(pane, mine_fetch_requested[5] && !later_fetch_in_flight ? "No watch later or unavailable" :
                      "Loading watch later...", 14, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 20);
            if (!mine_fetch_requested[5]) fetch_later_videos();
        } else if (mine_active_tab == 5) {
            for (size_t index = 0; index < displayed_later_videos.size() && index < 3; ++index) {
                const Video& item = displayed_later_videos[index];
                lv_obj_t* row = lv_obj_create(pane);
                lv_obj_set_size(row, 680, 46);
                lv_obj_set_pos(row, 16, 48 + static_cast<int>(index) * 50);
                lv_obj_set_style_bg_color(row, ui_color(0x202735), 0);
                lv_obj_set_style_border_width(row, 0, 0);
                lv_obj_set_style_radius(row, 4, 0);
                set_label(row, item.title.c_str(), 13, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 10, 0);
                lv_obj_t* remove = lv_btn_create(row);
                lv_obj_set_size(remove, 64, 28);
                lv_obj_align(remove, LV_ALIGN_RIGHT_MID, -8, 0);
                lv_obj_set_style_bg_color(remove, ui_color(0x8d3b45), 0);
                lv_obj_set_style_border_width(remove, 0, 0);
                lv_obj_add_event_cb(remove, remove_later_video, LV_EVENT_CLICKED,
                                    reinterpret_cast<void*>(static_cast<uintptr_t>(index)));
                set_label(remove, "Remove", 10, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
            }
        } else if (mine_active_tab == 2 && displayed_subscription_titles.empty()) {
            set_label(pane, mine_fetch_requested[2] && !subscription_fetch_in_flight ? "No subscriptions or unavailable" :
                      "Loading subscriptions...", 14, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 20);
            if (!mine_fetch_requested[2]) fetch_subscriptions();
        } else if (mine_active_tab == 2) {
            for (size_t index = 0; index < displayed_subscription_titles.size() && index < 3; ++index) {
                lv_obj_t* row = lv_obj_create(pane);
                lv_obj_set_size(row, 680, 46);
                lv_obj_set_pos(row, 16, 48 + static_cast<int>(index) * 50);
                lv_obj_set_style_bg_color(row, ui_color(0x202735), 0);
                lv_obj_set_style_border_width(row, 0, 0);
                lv_obj_set_style_radius(row, 4, 0);
                set_label(row, displayed_subscription_titles[index].c_str(), 13,
                          ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 10, 0);
            }
        } else if (mine_active_tab == 3 && displayed_bangumi_titles.empty()) {
            set_label(pane, mine_fetch_requested[3] && !bangumi_fetch_in_flight ? "No anime or unavailable" :
                      "Loading anime...", 14, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 20);
            if (!mine_fetch_requested[3]) fetch_bangumi_titles();
        } else if (mine_active_tab == 3) {
            for (size_t index = 0; index < displayed_bangumi_titles.size() && index < 3; ++index) {
                lv_obj_t* row = lv_obj_create(pane);
                lv_obj_set_size(row, 680, 46);
                lv_obj_set_pos(row, 16, 48 + static_cast<int>(index) * 50);
                lv_obj_set_style_bg_color(row, ui_color(0x202735), 0);
                lv_obj_set_style_border_width(row, 0, 0);
                lv_obj_set_style_radius(row, 4, 0);
                set_label(row, displayed_bangumi_titles[index].c_str(), 13,
                          ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 10, 0);
            }
        } else if (mine_active_tab == 4 && displayed_series_titles.empty()) {
            set_label(pane, mine_fetch_requested[4] && !series_fetch_in_flight ? "No series or unavailable" :
                      "Loading series...", 14, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 20);
            if (!mine_fetch_requested[4]) fetch_series_titles();
        } else if (mine_active_tab == 4) {
            for (size_t index = 0; index < displayed_series_titles.size() && index < 3; ++index) {
                lv_obj_t* row = lv_obj_create(pane);
                lv_obj_set_size(row, 680, 46);
                lv_obj_set_pos(row, 16, 48 + static_cast<int>(index) * 50);
                lv_obj_set_style_bg_color(row, ui_color(0x202735), 0);
                lv_obj_set_style_border_width(row, 0, 0);
                lv_obj_set_style_radius(row, 4, 0);
                set_label(row, displayed_series_titles[index].c_str(), 13,
                          ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 10, 0);
            }
        } else {
            set_label(pane, "No account section selected.", 14, ui_color(0xaeb8c6), LV_ALIGN_CENTER, 0, 20);
        }
        return;
    }

    if (section == MainSection::Inbox) {
        lv_label_set_text(status_label, account_name.empty() ? "Inbox / sign in required" : "Inbox / authenticated");
        const char* tabs[] = {"Chat", "Replies", "Mentions", "Likes"};
        for (unsigned int i = 0; i < 4; ++i) {
            board_tab_button(content, tabs[i], static_cast<int>(i) * 128, 120, i == inbox_active_tab,
                             select_inbox_tab, i);
        }
        lv_obj_t* pane = lv_obj_create(content);
        lv_obj_set_size(pane, 720, 332);
        lv_obj_set_pos(pane, 0, 50);
        lv_obj_set_style_bg_color(pane, ui_color(0x1d2330), 0);
        lv_obj_set_style_border_width(pane, 0, 0);
        lv_obj_set_style_radius(pane, 4, 0);
        lv_obj_set_scroll_dir(pane, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(pane, LV_SCROLLBAR_MODE_AUTO);
        if (account_name.empty()) {
            board_empty_state(pane, LV_SYMBOL_BELL, tabs[inbox_active_tab],
                              "Sign in to view notifications and messages");
        } else if (displayed_inbox_items.empty()) {
            set_label(pane, inbox_active_tab < inbox_fetch_requested.size() &&
                      inbox_fetch_requested[inbox_active_tab] && !inbox_fetch_in_flight ?
                      "No messages or unavailable" : "Loading messages...",
                      18, ui_color(0xf3f5f8), LV_ALIGN_CENTER, 0, 0);
            if (inbox_active_tab < inbox_fetch_requested.size() && !inbox_fetch_requested[inbox_active_tab])
                fetch_inbox_items(inbox_active_tab);
        } else {
            const bool showing_thread = inbox_active_tab == 0 && !active_chat_talker.empty() && !displayed_chat_messages.empty();
            const std::vector<InboxItem>& items = showing_thread ? displayed_chat_messages : displayed_inbox_items;
            if (showing_thread) {
                lv_obj_t* back = lv_btn_create(pane);
                lv_obj_set_size(back, 72, 30);
                lv_obj_set_pos(back, 14, 8);
                lv_obj_add_event_cb(back, close_chat_thread, LV_EVENT_CLICKED, nullptr);
                set_label(back, "Back", 12, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
                lv_obj_t* more = lv_btn_create(pane);
                lv_obj_set_size(more, 108, 30);
                lv_obj_set_pos(more, 96, 8);
                lv_obj_set_style_bg_color(more, ui_color(0x374152), 0);
                lv_obj_set_style_border_width(more, 0, 0);
                lv_obj_add_event_cb(more, load_more_chat_messages, LV_EVENT_CLICKED, nullptr);
                set_label(more, "More", 12, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
            }
            for (size_t index = 0; index < items.size(); ++index) {
                const InboxItem& item = items[index];
                lv_obj_t* row = lv_obj_create(pane);
                lv_obj_set_size(row, 692, 58);
                lv_obj_set_pos(row, 14, (showing_thread ? 46 : 10) + static_cast<int>(index) * 66);
                lv_obj_set_style_bg_color(row, ui_color(0x202735), 0);
                lv_obj_set_style_border_width(row, 0, 0);
                lv_obj_set_style_radius(row, 4, 0);
                if (!showing_thread && inbox_active_tab == 0 && !item.talker_id.empty()) {
                    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_add_event_cb(row, select_chat_session, LV_EVENT_CLICKED,
                                        reinterpret_cast<void*>(static_cast<uintptr_t>(index)));
                }
                set_label(row, item.sender.c_str(), 13, ui_color(0xf3f5f8), LV_ALIGN_LEFT_MID, 10, -10);
                const std::string meta = item.unread_count.empty() || item.unread_count == "0"
                    ? item.timestamp : "unread " + item.unread_count;
                if (!meta.empty()) set_label(row, meta.c_str(), 9, ui_color(0x7e8a9a), LV_ALIGN_RIGHT_MID, -8, -10);
                set_label(row, item.message.c_str(), 11, ui_color(0xaeb8c6), LV_ALIGN_LEFT_MID, 10, 14);
            }
        }
        return;
    }

    const char* categories[] = {"Playback", "Interface", "Other"};
    lv_label_set_text(status_label, "Settings / persistent subset");
    for (unsigned int i = 0; i < 3; ++i) {
        board_tab_button(content, categories[i], static_cast<int>(i) * 126, 116, i == settings_active_tab,
                         select_settings_tab, i);
    }
    lv_obj_t* pane = lv_obj_create(content);
    lv_obj_set_size(pane, 720, 320);
    lv_obj_set_pos(pane, 0, 50);
    lv_obj_set_style_bg_opa(pane, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pane, 0, 0);
    lv_obj_set_style_pad_all(pane, 0, 0);
    lv_obj_set_scroll_dir(pane, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(pane, LV_SCROLLBAR_MODE_AUTO);
    if (settings_active_tab == 0) {
        board_setting_switch(pane, "Hardware decoding", "Use the configured hardware decoder", &setting_hwdec, 0);
        board_setting_switch(pane, "Auto fullscreen", "Enter fullscreen on playback", &setting_autofullscreen, 76);
        board_setting_switch(pane, "Low quality mode", "Prefer a lower stream quality", &setting_low_quality, 152);
        lv_obj_t* quality = lv_btn_create(pane);
        lv_obj_set_size(quality, 720, 46);
        lv_obj_set_pos(quality, 0, 228);
        lv_obj_set_style_bg_color(quality, ui_color(0x202735), 0);
        lv_obj_set_style_border_width(quality, 0, 0);
        lv_obj_set_style_radius(quality, 4, 0);
        lv_obj_add_event_cb(quality, cycle_board_quality_profile, LV_EVENT_CLICKED, nullptr);
        const std::string profile = "Quality profile  qn " + std::to_string(setting_video_quality) +
                                     " / audio " + std::to_string(setting_audio_quality) +
                                     " / cache " + std::to_string(setting_memory_cache) + " MiB";
        set_label(quality, profile.c_str(), 12, ui_color(0xf3f5f8), LV_ALIGN_CENTER, 0, 0);
        const char* codec = setting_video_codec == 12 ? "HEVC/H.265" : setting_video_codec == 13 ? "AV1" : "AVC/H.264";
        lv_obj_t* format = lv_btn_create(pane);
        lv_obj_set_size(format, 720, 42);
        lv_obj_set_pos(format, 0, 282);
        lv_obj_set_style_bg_color(format, ui_color(0x202735), 0);
        lv_obj_set_style_border_width(format, 0, 0);
        lv_obj_add_event_cb(format, cycle_video_format, LV_EVENT_CLICKED, nullptr);
        set_label(format, (std::string("Format  ") + (setting_video_format == 4048 ? "DASH" : "FLV/MP4")).c_str(),
                  12, ui_color(0xffffff), LV_ALIGN_LEFT_MID, 12, 0);
        lv_obj_t* codec_button = lv_btn_create(pane);
        lv_obj_set_size(codec_button, 720, 42);
        lv_obj_set_pos(codec_button, 0, 332);
        lv_obj_set_style_bg_color(codec_button, ui_color(0x202735), 0);
        lv_obj_set_style_border_width(codec_button, 0, 0);
        lv_obj_add_event_cb(codec_button, cycle_video_codec, LV_EVENT_CLICKED, nullptr);
        set_label(codec_button, (std::string("Codec  ") + codec).c_str(), 12, ui_color(0xffffff), LV_ALIGN_LEFT_MID, 12, 0);
    } else if (settings_active_tab == 2) {
        proxy_input = nullptr;
        board_setting_switch(pane, "TLS verification", "Verify HTTPS certificates", &setting_tls_verify, 0);
        board_setting_switch(pane, "Danmaku", "Show timed scrolling comments", &setting_danmaku_on, 76);
        lv_obj_t* speed = lv_btn_create(pane);
        lv_obj_set_size(speed, 720, 42);
        lv_obj_set_pos(speed, 0, 152);
        lv_obj_set_style_bg_color(speed, ui_color(0x202735), 0);
        lv_obj_set_style_border_width(speed, 0, 0);
        lv_obj_add_event_cb(speed, cycle_danmaku_speed, LV_EVENT_CLICKED, nullptr);
        const std::string speed_label = "Danmaku speed (%)   " + std::to_string(setting_danmaku_speed);
        set_label(speed, speed_label.c_str(), 12, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        board_setting_switch(pane, "HTTP proxy", "Use saved proxy for HTTPS", &setting_http_proxy_enabled, 202);
        proxy_input = lv_textarea_create(pane);
        lv_obj_set_size(proxy_input, 720, 42);
        lv_obj_set_pos(proxy_input, 0, 278);
        lv_textarea_set_one_line(proxy_input, true);
        lv_obj_set_height(proxy_input, 42);
        lv_obj_set_style_pad_ver(proxy_input, 8, 0);
        lv_textarea_set_placeholder_text(proxy_input, "Proxy host:port, then press Enter");
        lv_textarea_set_text(proxy_input, setting_http_proxy.c_str());
        style_dark_textarea(proxy_input);
        lv_obj_add_event_cb(proxy_input, focus_http_proxy, LV_EVENT_FOCUSED, nullptr);
        lv_obj_add_event_cb(proxy_input, unfocus_http_proxy, LV_EVENT_DEFOCUSED, nullptr);
    } else {
        board_setting_switch(pane, "Show FPS", "Display the LVGL frame-rate indicator", &setting_show_fps, 0);
        board_setting_switch(pane, "Hide bottom bar", "Remove the navigation bar while browsing", &setting_hide_bottom_bar, 76);
        board_setting_switch(pane, "TV OSD mode", "Use compact live-TV controls", &setting_tv_osd, 152);
        lv_obj_t* card = lv_obj_create(pane);
        lv_obj_set_size(card, 720, 236);
        lv_obj_set_pos(card, 0, 230);
        lv_obj_set_style_bg_color(card, ui_color(0x1d2330), 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_radius(card, 4, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lock_static_canvas(card);
        set_label(card, "UI scale, frame rate and controls", 16, ui_color(0xf3f5f8),
                  LV_ALIGN_TOP_LEFT, 16, 14);
        lv_obj_t* scale = lv_btn_create(card);
        lv_obj_set_size(scale, 180, 36);
        lv_obj_set_pos(scale, 16, 54);
        lv_obj_add_event_cb(scale, cycle_ui_scale, LV_EVENT_CLICKED, nullptr);
        const std::string scale_text = "UI scale  " + std::to_string(setting_ui_scale) + "%";
        set_label(scale, scale_text.c_str(), 13, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        set_label(card, "Fixed 800x480 layout; UI scale is saved for host preview.",
                  12, ui_color(0xaeb8c6), LV_ALIGN_TOP_LEFT, 16, 98);
        lv_obj_t* fps_limit = lv_btn_create(card);
        lv_obj_set_size(fps_limit, 210, 36);
        lv_obj_set_pos(fps_limit, 16, 122);
        lv_obj_set_style_bg_color(fps_limit, ui_color(0x202735), 0);
        lv_obj_set_style_border_width(fps_limit, 0, 0);
        lv_obj_add_event_cb(fps_limit, cycle_limited_fps, LV_EVENT_CLICKED, nullptr);
        const std::string fps_text = setting_limited_fps == 0 ? "FPS unlimited" : "FPS " + std::to_string(setting_limited_fps);
        set_label(fps_limit, fps_text.c_str(), 12, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        lv_obj_t* swap = lv_btn_create(card);
        lv_obj_set_size(swap, 210, 36);
        lv_obj_set_pos(swap, 244, 122);
        lv_obj_set_style_bg_color(swap, ui_color(setting_swap_abxy ? 0x2877a9 : 0x202735), 0);
        lv_obj_set_style_border_width(swap, 0, 0);
        lv_obj_add_event_cb(swap, toggle_setting_button, LV_EVENT_CLICKED, &setting_swap_abxy);
        set_label(swap, setting_swap_abxy ? "A/B swapped" : "A/B standard", 12, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        lv_obj_t* keymap = lv_btn_create(card);
        lv_obj_set_size(keymap, 210, 36);
        lv_obj_set_pos(keymap, 472, 122);
        lv_obj_set_style_bg_color(keymap, ui_color(0x202735), 0);
        lv_obj_set_style_border_width(keymap, 0, 0);
        lv_obj_add_event_cb(keymap, cycle_keymap, LV_EVENT_CLICKED, nullptr);
        set_label(keymap, (std::string("Keymap  ") + setting_keymap).c_str(), 12, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        lv_obj_t* theme = lv_btn_create(card);
        lv_obj_set_size(theme, 210, 36);
        lv_obj_set_pos(theme, 16, 168);
        lv_obj_set_style_bg_color(theme, ui_color(0x202735), 0);
        lv_obj_set_style_border_width(theme, 0, 0);
        lv_obj_add_event_cb(theme, cycle_app_theme, LV_EVENT_CLICKED, nullptr);
        set_label(theme, (std::string("Theme  ") + setting_app_theme).c_str(), 12, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    }
}

void show_main_section(MainSection section) {
    // A section change invalidates any detached detail/auxiliary completion.
    // Without this guard a late detail response can rebuild the detail page
    // after Home has deleted it, freezing the LVGL event cycle.
    ++navigation_epoch;
    search_focused = false;
    active_text_input = nullptr;
    if (player_page_visible || player_playing) leave_player_page();
    main_section = section;
    update_main_nav_style();
    update_shell_for_section(section);
    if (section == MainSection::Home) {
        // A --player-bv/detail launch creates a one-item transport object.
        // It is not a valid Home feed and must never be reused as one after
        // the user leaves the player.
        if (displayed_videos.size() >= 2) show_recommend_page(displayed_videos, {});
        else refresh_recommend(nullptr);
        return;
    }
    if (board_preview) {
        show_board_section(section);
        return;
    }
    if (section == MainSection::Dynamic) {
        show_dynamic_page();
        return;
    }
    if (section == MainSection::Mine) {
        show_mine_page();
        return;
    }
    if (section == MainSection::Settings) {
        show_settings_page();
        return;
    }
    if (section == MainSection::Inbox) {
        show_inbox_page();
        return;
    }
    if (content) lv_obj_del(content);
    detail_visible = false;
    content = lv_obj_create(lv_scr_act());
    lv_obj_set_size(content, 1120, 590);
    lv_obj_set_pos(content, 130, 110);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    const char* names[] = {"Home", "Dynamic", "Mine", "Inbox", "Settings"};
    lv_label_set_text(status_label, names[static_cast<size_t>(section)]);
    lv_obj_t* title = lv_label_create(content);
    lv_label_set_text(title, names[static_cast<size_t>(section)]);
    lv_obj_set_style_text_font(title, ui_font(), 0);
    lv_obj_set_style_text_color(title, ui_color(0xf3f5f8), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 10);
}

void select_main_section(lv_event_t* event) {
    const auto section = static_cast<MainSection>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
    if (section == MainSection::Dynamic) {
        dynamic_fetch_requested = false;
        dynamic_up_fetch_requested = false;
    } else if (section == MainSection::Mine && mine_active_tab < mine_fetch_requested.size()) {
        mine_fetch_requested[mine_active_tab] = false;
    } else if (section == MainSection::Inbox && inbox_active_tab < inbox_fetch_requested.size()) {
        inbox_fetch_requested[inbox_active_tab] = false;
    }
    show_main_section(section);
}

void build_shell() {
    lv_obj_t* root = lv_scr_act();
    lv_obj_set_size(root, window_width, window_height);
    lv_obj_set_style_bg_color(root, ui_color(0x151922), 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    // The 800x480 target is a fixed DE canvas, not a scrollable desktop page.
    // Prevent any child rounding or font metrics from creating root scrollbars.
    lock_static_canvas(root);
    fps_window_start = platform_ticks();
    fps_frame_count = 0;
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(root, 0, LV_PART_SCROLLBAR);
    if (board_preview) {
        lv_obj_t* rail = lv_obj_create(root);
        nav_rail = rail;
        if (setting_hide_bottom_bar) lv_obj_add_flag(rail, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(rail, 64, window_height);
        lv_obj_set_style_bg_color(rail, ui_color(0x111827), 0);
        lv_obj_set_style_border_width(rail, 0, 0);
        lv_obj_set_style_pad_all(rail, 0, 0);
        lock_static_canvas(rail);
        set_label(rail, "W", 20, ui_color(0xffffff), LV_ALIGN_TOP_MID, 0, 18);
        // Upstream main.xml uses icon-only tabs. Keep the 64px board rail
        // icon-like rather than truncating desktop words inside it.
        const char* primary_nav[] = {LV_SYMBOL_HOME, LV_SYMBOL_LIST, LV_SYMBOL_DIRECTORY};
        for (int i = 0; i < 3; ++i) {
            lv_obj_t* nav = lv_btn_create(rail);
            lv_obj_set_size(nav, 50, 38);
            lv_obj_set_pos(nav, 7, 94 + i * 46);
            lv_obj_set_style_bg_color(nav, ui_color(i == 0 ? 0x0ea5e9 : 0x1f2937), 0);
            lv_obj_set_style_radius(nav, 6, 0);
            lv_obj_set_style_border_width(nav, 0, 0);
            main_nav[i] = nav;
            lv_obj_add_event_cb(nav, select_main_section, LV_EVENT_CLICKED,
                                reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
            set_symbol(nav, primary_nav[i], ui_color(0xffffff));
        }
        lv_obj_t* inbox = lv_btn_create(rail);
        lv_obj_set_size(inbox, 50, 38);
        lv_obj_set_pos(inbox, 7, 244);
        lv_obj_set_style_bg_color(inbox, ui_color(0x1f2937), 0);
        lv_obj_set_style_radius(inbox, 6, 0);
        lv_obj_set_style_border_width(inbox, 0, 0);
        lv_obj_add_event_cb(inbox, select_main_section, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(MainSection::Inbox)));
        main_nav[static_cast<size_t>(MainSection::Inbox)] = inbox;
        set_symbol(inbox, LV_SYMBOL_BELL, ui_color(0xffffff));
        lv_obj_t* settings = lv_btn_create(rail);
        lv_obj_set_size(settings, 50, 38);
        lv_obj_set_pos(settings, 7, 290);
        lv_obj_set_style_bg_color(settings, ui_color(0x1f2937), 0);
        lv_obj_set_style_radius(settings, 6, 0);
        lv_obj_set_style_border_width(settings, 0, 0);
        lv_obj_add_event_cb(settings, select_main_section, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(MainSection::Settings)));
        main_nav[static_cast<size_t>(MainSection::Settings)] = settings;
        set_symbol(settings, LV_SYMBOL_SETTINGS, ui_color(0xffffff));
        lv_obj_t* top = lv_obj_create(root);
        lv_obj_set_size(top, 736, 72);
        lv_obj_set_pos(top, 64, 0);
        lv_obj_set_style_bg_color(top, ui_color(0x111827), 0);
        lv_obj_set_style_border_width(top, 0, 0);
        lock_static_canvas(top);
        top_title = lv_label_create(top);
        lv_label_set_text(top_title, "Home");
        lv_obj_set_style_text_font(top_title, ui_font(), 0);
        lv_obj_set_style_text_color(top_title, ui_color(0xf3f5f8), 0);
        lv_obj_align(top_title, LV_ALIGN_LEFT_MID, 10, 0);
        lv_obj_t* previous = lv_btn_create(top);
        lv_obj_set_size(previous, 32, 32);
        lv_obj_set_pos(previous, 76, 20);
        lv_obj_set_style_bg_color(previous, ui_color(0x1f2937), 0);
        lv_obj_set_style_radius(previous, 6, 0);
        lv_obj_set_style_border_width(previous, 0, 0);
        lv_obj_add_event_cb(previous, previous_feed_page, LV_EVENT_CLICKED, nullptr);
        set_label(previous, "<", 15, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        home_header_controls.push_back(previous);
        lv_obj_t* next = lv_btn_create(top);
        lv_obj_set_size(next, 32, 32);
        lv_obj_set_pos(next, 116, 20);
        lv_obj_set_style_bg_color(next, ui_color(0x0ea5e9), 0);
        lv_obj_set_style_radius(next, 6, 0);
        lv_obj_set_style_border_width(next, 0, 0);
        lv_obj_add_event_cb(next, next_feed_page, LV_EVENT_CLICKED, nullptr);
        set_label(next, ">", 15, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        home_header_controls.push_back(next);
        search_input = lv_textarea_create(top);
        lv_obj_set_size(search_input, 330, 36);
        lv_obj_set_pos(search_input, 210, 18);
        lv_textarea_set_one_line(search_input, true);
        lv_textarea_set_placeholder_text(search_input, "Search Bilibili");
        if (!search_term.empty()) lv_textarea_set_text(search_input, search_term.c_str());
        lv_obj_set_style_bg_color(search_input, ui_color(0x1f2937), 0);
        lv_obj_set_style_text_color(search_input, ui_color(0xf3f5f8), 0);
        lv_obj_set_style_border_width(search_input, 0, 0);
        lv_obj_set_style_radius(search_input, 4, 0);
        lv_obj_set_style_text_font(search_input, ui_font(), 0);
        lv_obj_add_event_cb(search_input, focus_search, LV_EVENT_FOCUSED, nullptr);
        lv_obj_add_event_cb(search_input, unfocus_search, LV_EVENT_DEFOCUSED, nullptr);
        home_header_controls.push_back(search_input);
        lv_obj_t* search = lv_btn_create(top);
        lv_obj_set_size(search, 112, 36);
        lv_obj_set_pos(search, 554, 18);
        lv_obj_set_style_bg_color(search, ui_color(0x0ea5e9), 0);
        lv_obj_set_style_radius(search, 6, 0);
        lv_obj_set_style_border_width(search, 0, 0);
        lv_obj_add_event_cb(search, submit_search, LV_EVENT_CLICKED, nullptr);
        set_label(search, "Search", 13, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
        home_header_controls.push_back(search);
        status_label = lv_label_create(root);
        lv_obj_set_width(status_label, 720);
        lv_obj_set_pos(status_label, 72, 72);
        lv_obj_set_style_text_font(status_label, ui_font(), 0);
        lv_obj_set_style_text_color(status_label, ui_color(0xb8c3cf), 0);
        if (setting_show_fps) {
            fps_label = lv_label_create(root);
            lv_obj_set_style_text_font(fps_label, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(fps_label, ui_color(0x8fd3ff), 0);
            lv_obj_align(fps_label, LV_ALIGN_TOP_RIGHT, -8, 8);
            lv_label_set_text(fps_label, "FPS --");
        }
        return;
    }
    lv_obj_t* rail = lv_obj_create(root);
    nav_rail = rail;
    if (setting_hide_bottom_bar) lv_obj_add_flag(rail, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(rail, 100, kWindowHeight);
    lv_obj_set_style_bg_color(rail, ui_color(0x202633), 0);
    lv_obj_set_style_border_width(rail, 0, 0);
    lv_obj_set_style_radius(rail, 0, 0);
    // The desktop rail is fixed navigation, never a scroll container.  LVGL
    // otherwise reserves and paints an idle scrollbar at its bottom.
    lock_static_canvas(rail);
    set_label(rail, "wiliwili", 24, ui_color(0xffffff), LV_ALIGN_TOP_MID, 0, 34);
    const char* primary_nav[] = {"Home", "Dynamic", "Mine", "Inbox", "Settings"};
    for (int i = 0; i < 5; ++i) {
        lv_obj_t* nav = lv_btn_create(rail);
        lv_obj_set_size(nav, 76, 42);
        lv_obj_align(nav, LV_ALIGN_TOP_MID, 0, 105 + i * 52);
        lv_obj_set_style_bg_color(nav, ui_color(i == 0 ? 0x2877a9 : 0x2b3444), 0);
        lv_obj_set_style_radius(nav, 6, 0);
        lv_obj_set_style_border_width(nav, 0, 0);
        main_nav[i] = nav;
        lv_obj_add_event_cb(nav, select_main_section, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
        set_label(nav, primary_nav[i], 16, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    }

    lv_obj_t* top = lv_obj_create(root);
    lv_obj_set_size(top, 1180, 86);
    lv_obj_set_pos(top, 100, 0);
    lv_obj_set_style_bg_color(top, ui_color(0x151922), 0);
    lv_obj_set_style_border_width(top, 0, 0);
    lv_obj_set_style_radius(top, 0, 0);
    top_title = lv_label_create(top);
    lv_label_set_text(top_title, "Home");
    lv_obj_set_style_text_font(top_title, ui_font(), 0);
    lv_obj_set_style_text_color(top_title, ui_color(0xf3f5f8), 0);
    lv_obj_align(top_title, LV_ALIGN_LEFT_MID, 32, 0);
    lv_obj_t* live = lv_label_create(top);
    lv_label_set_text(live, "Live");
    lv_obj_set_style_text_font(live, ui_font(), 0);
    lv_obj_set_style_text_color(live, ui_color(0x58b9ee), 0);
    lv_obj_align(live, LV_ALIGN_LEFT_MID, 120, 0);
    home_header_controls.push_back(live);
    recommend_tab = lv_btn_create(top);
    lv_obj_set_size(recommend_tab, 116, 38);
    lv_obj_align(recommend_tab, LV_ALIGN_LEFT_MID, 175, 0);
    lv_obj_set_style_radius(recommend_tab, 6, 0);
    lv_obj_add_event_cb(recommend_tab, select_recommend, LV_EVENT_CLICKED, nullptr);
    set_label(recommend_tab, "Recommend", 16, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    home_header_controls.push_back(recommend_tab);
    popular_tab = lv_btn_create(top);
    lv_obj_set_size(popular_tab, 94, 38);
    lv_obj_align(popular_tab, LV_ALIGN_LEFT_MID, 300, 0);
    lv_obj_set_style_radius(popular_tab, 6, 0);
    lv_obj_add_event_cb(popular_tab, select_popular, LV_EVENT_CLICKED, nullptr);
    set_label(popular_tab, "Popular", 16, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    home_header_controls.push_back(popular_tab);
    update_feed_tab_style();
    lv_obj_t* bangumi = lv_label_create(top);
    lv_label_set_text(bangumi, "Bangumi");
    lv_obj_set_style_text_font(bangumi, ui_font(), 0);
    lv_obj_set_style_text_color(bangumi, ui_color(0x58b9ee), 0);
    lv_obj_align(bangumi, LV_ALIGN_LEFT_MID, 410, 0);
    home_header_controls.push_back(bangumi);
    lv_obj_t* cinema = lv_label_create(top);
    lv_label_set_text(cinema, "Cinema");
    lv_obj_set_style_text_font(cinema, ui_font(), 0);
    lv_obj_set_style_text_color(cinema, ui_color(0x58b9ee), 0);
    lv_obj_align(cinema, LV_ALIGN_LEFT_MID, 505, 0);
    home_header_controls.push_back(cinema);
    lv_obj_t* previous = lv_btn_create(top);
    lv_obj_set_size(previous, 38, 36);
    lv_obj_align(previous, LV_ALIGN_LEFT_MID, 595, 0);
    lv_obj_set_style_bg_color(previous, ui_color(0x1f2937), 0);
    lv_obj_set_style_radius(previous, 6, 0);
    lv_obj_set_style_border_width(previous, 0, 0);
    lv_obj_add_event_cb(previous, previous_feed_page, LV_EVENT_CLICKED, nullptr);
    set_label(previous, "<", 16, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    home_header_controls.push_back(previous);
    lv_obj_t* next = lv_btn_create(top);
    lv_obj_set_size(next, 38, 36);
    lv_obj_align(next, LV_ALIGN_LEFT_MID, 641, 0);
    lv_obj_set_style_bg_color(next, ui_color(0x0ea5e9), 0);
    lv_obj_set_style_radius(next, 6, 0);
    lv_obj_set_style_border_width(next, 0, 0);
    lv_obj_add_event_cb(next, next_feed_page, LV_EVENT_CLICKED, nullptr);
    set_label(next, ">", 16, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    home_header_controls.push_back(next);
    search_input = lv_textarea_create(top);
    lv_obj_set_size(search_input, 190, 36);
    lv_obj_align(search_input, LV_ALIGN_RIGHT_MID, -188, 0);
    lv_textarea_set_one_line(search_input, true);
    lv_textarea_set_placeholder_text(search_input, "Search videos");
    if (!search_term.empty()) lv_textarea_set_text(search_input, search_term.c_str());
    lv_obj_set_style_bg_color(search_input, ui_color(0x2b3444), 0);
    lv_obj_set_style_text_color(search_input, ui_color(0xf3f5f8), 0);
    lv_obj_set_style_border_width(search_input, 0, 0);
    lv_obj_set_style_radius(search_input, 6, 0);
    lv_obj_add_event_cb(search_input, focus_search, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(search_input, unfocus_search, LV_EVENT_DEFOCUSED, nullptr);
    home_header_controls.push_back(search_input);
    lv_obj_t* search = lv_btn_create(top);
    lv_obj_set_size(search, 72, 36);
    lv_obj_align(search, LV_ALIGN_RIGHT_MID, -108, 0);
    lv_obj_set_style_bg_color(search, ui_color(0x374152), 0);
    lv_obj_set_style_radius(search, 6, 0);
    lv_obj_set_style_border_width(search, 0, 0);
    lv_obj_add_event_cb(search, submit_search, LV_EVENT_CLICKED, nullptr);
    set_label(search, "Search", 14, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    home_header_controls.push_back(search);
    lv_obj_t* refresh = lv_btn_create(top);
    lv_obj_set_size(refresh, 72, 38);
    lv_obj_align(refresh, LV_ALIGN_RIGHT_MID, -28, 0);
    lv_obj_set_style_bg_color(refresh, ui_color(0x2877a9), 0);
    lv_obj_set_style_radius(refresh, 6, 0);
    lv_obj_add_event_cb(refresh, refresh_recommend, LV_EVENT_CLICKED, nullptr);
    set_label(refresh, "Refresh", 16, ui_color(0xffffff), LV_ALIGN_CENTER, 0, 0);
    home_header_controls.push_back(refresh);
    status_label = lv_label_create(root);
    lv_obj_set_width(status_label, 1120);
    lv_obj_set_style_text_font(status_label, ui_font(), 0);
    lv_obj_set_style_text_color(status_label, ui_color(0xb8c3cf), 0);
    lv_obj_set_pos(status_label, 130, 88);
    lv_label_set_text(status_label, "Loading the real Bilibili Home Recommend feed...");
    if (setting_show_fps) {
        fps_label = lv_label_create(root);
        lv_obj_set_style_text_font(fps_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(fps_label, ui_color(0x8fd3ff), 0);
        lv_obj_align(fps_label, LV_ALIGN_TOP_RIGHT, -10, 8);
        lv_label_set_text(fps_label, "FPS --");
    }
}

void start_fetch() {
    const uint32_t request_epoch = ++navigation_epoch;
    const FeedMode mode = feed_mode;
    const std::string query = search_term;
#ifdef WILIWILI_BOARD
    const int image_threads = 1;
#else
    const int image_threads = setting_image_threads;
#endif
    const int page = feed_page;
    const bool append = feed_append;
    std::thread([mode, query, image_threads, page, append, request_epoch] {
        std::string body;
        std::string error;
        std::vector<Video> videos;
        const bool popular = mode == FeedMode::Popular;
        std::string path = popular ? "/x/web-interface/popular?pn=" + std::to_string(page) + "&ps=8" :
                                     "/x/web-interface/wbi/index/top/feed/rcmd?fresh_idx=" + std::to_string(page) +
                                     "&ps=8&feed_version=V1&fresh_type=0&plat=1&x_num=3&y_num=4";
        if (mode == FeedMode::Search) {
            path = "/x/web-interface/search/type?search_type=video&page=" + std::to_string(page) + "&page_size=8&keyword=" +
                   url_encode(query);
        }
        if (fetch_feed(path, body, error)) {
            videos = mode == FeedMode::Search ? parse_search(body) : parse_feed(body, popular);
            if (videos.empty()) error = "Bilibili response could not be parsed";
            // Bound in-flight image transfers. The selected setting controls
            // work parallelism without retaining extra decoded surfaces.
            size_t retained_cover_bytes = 0;
            for (size_t first = 0; first < videos.size(); first += static_cast<size_t>(image_threads)) {
                if (request_epoch != navigation_epoch.load() || retained_cover_bytes >= kEncodedThumbnailBatchBytes) break;
#ifndef WILIWILI_BOARD
                std::vector<std::future<void>> jobs;
#endif
                const size_t end = std::min(videos.size(), first + static_cast<size_t>(image_threads));
#ifndef WILIWILI_BOARD
                jobs.reserve(end - first);
#endif
                size_t remaining = kEncodedThumbnailBatchBytes - retained_cover_bytes;
                for (size_t index = first; index < end; ++index) {
                    const size_t byte_limit = std::min(kMaxEncodedThumbnailBytes, remaining);
                    if (!byte_limit) break;
                    remaining -= byte_limit;
#ifdef WILIWILI_BOARD
                    fetch_thumbnail(videos[index].pic, videos[index].image_bytes, byte_limit);
#else
                    jobs.emplace_back(std::async(std::launch::async, [&videos, index, byte_limit] {
                        fetch_thumbnail(videos[index].pic, videos[index].image_bytes, byte_limit);
                    }));
#endif
                }
#ifndef WILIWILI_BOARD
                for (auto& job : jobs) job.get();
#endif
                for (size_t index = first; index < end; ++index) retained_cover_bytes += videos[index].image_bytes.capacity();
            }
        }
        std::lock_guard<std::mutex> lock(data_mutex);
        pending_videos = std::move(videos);
        pending_error = std::move(error);
        pending_feed_append = append;
        pending_feed_epoch = request_epoch;
        fetch_in_flight = false;
        fetch_complete = true;
    }).detach();
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 2 && std::string(argv[1]) == "--cookie-file") {
        cookie_import_path = argv[2];
    } else if (argc > 2 && std::string(argv[1]) == "--h264-test") {
        h264_test_uri = argv[2];
        open_player_after_first_feed = true;
    } else if (argc > 2 && std::string(argv[1]) == "--detail") {
        launch_bvid = argv[2];
        // A visual/detail invocation must remain a detail page even when the
        // user's normal-click Auto play preference is enabled.
        force_detail_view = true;
    } else if (argc > 2 && std::string(argv[1]) == "--season") {
        launch_season_id = argv[2];
    } else if (argc > 2 && std::string(argv[1]) == "--player-bv") {
        launch_bvid = argv[2];
        open_player_after_detail = true;
    } else if (argc > 2 && std::string(argv[1]) == "--player-bv-parts") {
        launch_bvid = argv[2];
        open_player_after_detail = true;
        open_parts_after_detail = true;
    } else if (argc > 2 && std::string(argv[1]) == "--player-bv-related") {
        launch_bvid = argv[2];
        open_player_after_detail = true;
        open_related_after_detail = true;
    } else if (argc > 1 && std::string(argv[1]) == "--popular") {
        popular_selected = true;
        feed_mode = FeedMode::Popular;
    } else if (argc > 2 && std::string(argv[1]) == "--search") {
        feed_mode = FeedMode::Search;
        search_term = argv[2];
    } else if (argc > 1 && std::string(argv[1]) == "--dynamic") {
        main_section = MainSection::Dynamic;
    } else if (argc > 1 && std::string(argv[1]) == "--mine") {
        main_section = MainSection::Mine;
    } else if (argc > 1 && std::string(argv[1]) == "--qr-login") {
        main_section = MainSection::Mine;
        open_qr_on_start = true;
    } else if (argc > 1 && std::string(argv[1]) == "--settings") {
        main_section = MainSection::Settings;
    } else if (argc > 1 && std::string(argv[1]) == "--inbox") {
        main_section = MainSection::Inbox;
    } else if (argc > 1 && std::string(argv[1]) == "--history") {
        feed_mode = FeedMode::Search;
        open_search_history_on_start = true;
    } else if (argc > 1 && std::string(argv[1]) == "--search-hots") {
        feed_mode = FeedMode::Search;
        open_search_hots_on_start = true;
    } else if (argc > 2 && std::string(argv[1]) == "--search-bangumi") {
        feed_mode = FeedMode::Search;
        search_term = argv[2];
        media_search_category = "media_bangumi";
        open_media_search_on_start = true;
    } else if (argc > 2 && std::string(argv[1]) == "--search-cinema") {
        feed_mode = FeedMode::Search;
        search_term = argv[2];
        media_search_category = "media_ft";
        open_media_search_on_start = true;
    } else if (argc > 1 && std::string(argv[1]) == "--player") {
        open_player_after_first_feed = true;
    } else if (argc > 1 && std::string(argv[1]) == "--board-preview") {
        board_preview = true;
        window_width = kBoardWidth;
        window_height = kBoardHeight;
    } else if (argc > 1 && std::string(argv[1]) == "--board-detail") {
        board_preview = true;
        window_width = kBoardWidth;
        window_height = kBoardHeight;
        open_detail_after_first_feed = true;
    } else if (argc > 1 && std::string(argv[1]) == "--board-player") {
        board_preview = true;
        window_width = kBoardWidth;
        window_height = kBoardHeight;
        open_player_after_first_feed = true;
    } else if (argc > 1 && std::string(argv[1]) == "--board-dynamic") {
        board_preview = true;
        window_width = kBoardWidth;
        window_height = kBoardHeight;
        main_section = MainSection::Dynamic;
    } else if (argc > 1 && std::string(argv[1]) == "--board-mine") {
        board_preview = true;
        window_width = kBoardWidth;
        window_height = kBoardHeight;
        main_section = MainSection::Mine;
    } else if (argc > 1 && std::string(argv[1]) == "--board-inbox") {
        board_preview = true;
        window_width = kBoardWidth;
        window_height = kBoardHeight;
        main_section = MainSection::Inbox;
    } else if (argc > 1 && std::string(argv[1]) == "--board-settings") {
        board_preview = true;
        window_width = kBoardWidth;
        window_height = kBoardHeight;
        main_section = MainSection::Settings;
    } else if (argc > 1 && std::string(argv[1]) == "--board-history") {
        board_preview = true;
        window_width = kBoardWidth;
        window_height = kBoardHeight;
        feed_mode = FeedMode::Search;
        open_search_history_on_start = true;
    } else if (argc > 2 && std::string(argv[1]) == "--board-search") {
        board_preview = true;
        window_width = kBoardWidth;
        window_height = kBoardHeight;
        feed_mode = FeedMode::Search;
        search_term = argv[2];
    }
#ifdef WILIWILI_BOARD
    board_preview = true;
    window_width = kBoardWidth;
    window_height = kBoardHeight;
#endif
    load_lvgl_config();
    platform_set_http_proxy(setting_http_proxy, setting_http_proxy_enabled);
    platform_set_tls_verify(setting_tls_verify);
    platform_set_http_timeouts(static_cast<uint32_t>(setting_http_connect_timeout_seconds * 1000),
                               static_cast<uint32_t>(setting_http_timeout_seconds * 1000));
    platform_set_gamepad_ab_swap(setting_swap_abxy);
    load_cookie();
    if (!cookie_import_path.empty()) {
        std::ifstream input(cookie_import_path, std::ios::binary);
        std::getline(input, bilibili_cookie);
        save_cookie();
    }
    load_search_history();
    if (feed_mode == FeedMode::Search && !search_term.empty()) add_search_history(search_term);
    // Create the LVGL-owned config on first run and normalize the currently
    // migrated subset before any page is shown.
    save_lvgl_config();
    probe_account();
    if (!platform_display_init(window_width, window_height, "wiliwili LVGL")) return 1;
    lv_init();
    focus_group = lv_group_create();
    lv_group_set_default(focus_group);
    platform_font_init();
    const size_t draw_pixels = static_cast<size_t>(window_width) * 16U;
    std::vector<lv_color_t> draw_a(draw_pixels);
    lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, draw_a.data(), nullptr, draw_a.size());
    lv_disp_drv_t display;
    lv_disp_drv_init(&display);
    display.hor_res = window_width;
    display.ver_res = window_height;
    display.draw_buf = &draw_buf;
    display.flush_cb = flush;
    lv_disp_drv_register(&display);
    lv_indev_drv_t pointer;
    lv_indev_drv_init(&pointer);
    pointer.type = LV_INDEV_TYPE_POINTER;
    pointer.read_cb = read_pointer;
    lv_indev_t* pointer_device = lv_indev_drv_register(&pointer);
    build_shell();
    platform_start_text_input();
    if (main_section != MainSection::Home) show_main_section(main_section);
    else if (!launch_bvid.empty()) {
        Video video;
        video.bvid = launch_bvid;
        displayed_videos.push_back(std::move(video));
        request_video_detail(0);
    }
    else if (!launch_season_id.empty()) fetch_season_detail(launch_season_id);
    else if (open_search_hots_on_start) fetch_search_hots();
    else if (open_media_search_on_start) fetch_media_search(media_search_category.c_str());
    else if (open_search_history_on_start) show_search_history_page();
    else start_fetch();
    if (open_qr_on_start) fetch_qr_login();
    apply_ui_scale();

    bool running = true;
    bool displayed = false;
    uint32_t last_tick = platform_ticks();
    while (running) {
        PlatformEvent event;
        while (platform_poll_event(event)) {
            if (event.type == PlatformEventType::Quit) running = false;
            if (event.type == PlatformEventType::TextInput && search_focused && active_text_input) {
                lv_textarea_add_text(active_text_input, event.text.c_str());
            }
            if (event.type == PlatformEventType::KeyDown && search_focused && active_text_input) {
                if (event.key == PlatformKey::Backspace) lv_textarea_del_char(active_text_input);
                if (event.key == PlatformKey::Enter) {
                    if (active_text_input == detail_comment_input) submit_video_comment(nullptr);
                    else if (active_text_input == inbox_message_input) send_chat_message(nullptr);
                    else if (active_text_input == proxy_input) save_http_proxy(nullptr);
                    else submit_search(nullptr);
                }
            }
            if (event.type == PlatformEventType::KeyDown && event.key == PlatformKey::Escape) {
                if (fullscreen_active) {
                    platform_display_set_fullscreen(false);
                    fullscreen_active = false;
                } else if (season_page_visible) return_to_media_search(nullptr);
                else if (detail_visible) return_to_recommend(nullptr);
                else running = false;
            }
            if (event.type == PlatformEventType::KeyDown && event.key == PlatformKey::Fullscreen && !board_preview) {
                fullscreen_active = !fullscreen_active;
                platform_display_set_fullscreen(fullscreen_active);
            }
            if (!search_focused && event.type == PlatformEventType::KeyDown && focus_group) {
                if (event.key == PlatformKey::Left || event.key == PlatformKey::Up) {
                    lv_group_focus_prev(focus_group);
                } else if (event.key == PlatformKey::Right || event.key == PlatformKey::Down) {
                    lv_group_focus_next(focus_group);
                } else if (event.key == PlatformKey::Enter) {
                    lv_obj_t* focused = lv_group_get_focused(focus_group);
                    if (focused) lv_event_send(focused, LV_EVENT_CLICKED, nullptr);
                }
            }
            if (!search_focused && event.type == PlatformEventType::KeyDown && event.key == PlatformKey::Refresh) refresh_recommend(nullptr);
            if (!search_focused && event.type == PlatformEventType::KeyDown && event.key == PlatformKey::Popular) select_popular(nullptr);
            if (!search_focused && event.type == PlatformEventType::KeyDown && event.key == PlatformKey::Home) select_recommend(nullptr);
            if (event.type == PlatformEventType::PointerMove) {
                pointer_data.point.x = event.x;
                pointer_data.point.y = event.y;
            }
            if (event.type == PlatformEventType::PointerButton) {
                pointer_data.point.x = event.x;
                pointer_data.point.y = event.y;
                pointer_data.state = event.pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
                lv_indev_read_timer_cb(pointer_device->driver->read_timer);
            }
            if (event.type == PlatformEventType::PointerWheel && event.wheel_y != 0) {
                pointer_data.point.x = event.x;
                pointer_data.point.y = event.y;
                lv_obj_update_layout(lv_scr_act());
                lv_obj_t* target = scroll_target_at(lv_scr_act(), pointer_data.point, event.wheel_y);
                if (target) lv_obj_scroll_by_bounded(target, 0, wiliwili::clamp(event.wheel_y, -20, 20) * 32, LV_ANIM_OFF);
            }
        }
        const uint32_t now = platform_ticks();
        lv_tick_inc(now - last_tick);
        last_tick = now;
        if (fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            if (pending_feed_epoch != navigation_epoch.load() || main_section != MainSection::Home) {
                pending_videos.clear();
                pending_error.clear();
                pending_feed_append = false;
            } else {
                if (content) {
                    lv_obj_del(content);
                    content = nullptr;
                }
                if (pending_feed_append) {
                    std::vector<Video> added_videos;
                    added_videos.reserve(pending_videos.size());
                    for (Video& video : pending_videos) {
                        if (displayed_videos.size() + added_videos.size() >= kMaxFeedVideos) break;
                        const auto same_video = [&video](const Video& current) {
                            return !video.bvid.empty() && current.bvid == video.bvid;
                        };
                        if (!std::any_of(displayed_videos.begin(), displayed_videos.end(), same_video) &&
                            !std::any_of(added_videos.begin(), added_videos.end(), same_video)) {
                            added_videos.push_back(std::move(video));
                        }
                    }
                    append_thumbnails(added_videos);
                    for (Video& video : added_videos) displayed_videos.push_back(std::move(video));
                    if (feed_mode == FeedMode::Search) show_search_results_page(displayed_videos, pending_error);
                    else show_recommend_page(displayed_videos, pending_error);
                } else {
                    prepare_thumbnails(pending_videos);
                    if (feed_mode == FeedMode::Search) show_search_results_page(pending_videos, pending_error);
                    else show_recommend_page(pending_videos, pending_error);
                }
                pending_feed_append = false;
                feed_append = false;
                if (open_player_after_first_feed && !pending_videos.empty()) {
                    open_player_after_first_feed = false;
                    show_player_page(0);
                }
                if (open_detail_after_first_feed && !pending_videos.empty()) {
                    open_detail_after_first_feed = false;
                    request_video_detail(0);
                }
                pending_videos.clear();
                displayed = true;
            }
        }
        if (detail_fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            if (pending_detail_epoch != navigation_epoch.load()) {
                // The user left the detail route before this request finished.
            } else if (!pending_detail_error.empty()) {
                if (status_label) lv_label_set_text(status_label, pending_detail_error.c_str());
            } else if (pending_detail_index < displayed_videos.size()) {
                displayed_videos[pending_detail_index] = std::move(pending_detail);
                active_part_index = displayed_videos[pending_detail_index].parts.empty() ? 0 :
                                    std::min(pending_detail_part_index,
                                             displayed_videos[pending_detail_index].parts.size() - 1);
                // Normal card selection always opens details first.  Player
                // launch is reserved for an explicit Play action or test mode.
                if (open_player_after_detail) {
                    open_player_after_detail = false;
                    if (open_parts_after_detail) {
                        open_parts_after_detail = false;
                        player_side_panel = PlayerSidePanel::Parts;
                    }
                    if (open_related_after_detail) {
                        open_related_after_detail = false;
                        player_side_panel = PlayerSidePanel::Related;
                    }
                    show_player_page(pending_detail_index);
                } else {
                    show_video_detail(pending_detail_index);
                }
            }
        }
        if (detail_aux_fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            if (pending_detail_aux_epoch == navigation_epoch.load() &&
                pending_detail_aux_index < displayed_videos.size() &&
                displayed_videos[pending_detail_aux_index].bvid == pending_detail_aux.bvid) {
                displayed_videos[pending_detail_aux_index].comments = std::move(pending_detail_aux.comments);
                displayed_videos[pending_detail_aux_index].comments_exhausted = false;
                displayed_videos[pending_detail_aux_index].related = std::move(pending_detail_aux.related);
                comment_next_page = 2;
                if (player_page_visible && pending_detail_aux_index == active_video_index &&
                    (player_side_panel == PlayerSidePanel::Comments || player_side_panel == PlayerSidePanel::Related)) {
                    show_player_page(active_video_index);
                }
            }
        }
        if (comments_more_fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            comments_more_fetch_in_flight = false;
            if (merge_comment_page(pending_more_comments)) {
                if (player_page_visible && player_side_panel == PlayerSidePanel::Comments)
                    show_player_page(active_video_index);
            } else if (pending_more_comments.epoch == navigation_epoch.load() &&
                       pending_more_comments.video_index == active_video_index &&
                       active_video_index < displayed_videos.size() &&
                       displayed_videos[active_video_index].bvid == pending_more_comments.bvid &&
                       !pending_more_comments.error.empty()) {
                if (status_label) lv_label_set_text(status_label, pending_more_comments.error.c_str());
            }
            pending_more_comments = {};
        }
        if (hot_fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            displayed_hot_terms = std::move(pending_hot_terms);
            if (pending_hot_epoch == navigation_epoch.load()) show_search_hots_page();
        }
        if (media_fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            displayed_media_results = std::move(pending_media_results);
            if (pending_media_epoch == navigation_epoch.load()) show_media_search_page();
        }
        if (season_fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            displayed_season = std::move(pending_season);
            if (pending_season_epoch == navigation_epoch.load()) show_season_detail_page();
        }
        if (account_fetch_complete.exchange(false)) {
            mine_fetch_requested.fill(false);
            inbox_fetch_requested.fill(false);
            dynamic_fetch_requested = false;
            dynamic_up_fetch_requested = false;
            if (main_section == MainSection::Mine) {
                if (board_preview) show_main_section(MainSection::Mine);
                else show_mine_page();
            } else if (main_section == MainSection::Dynamic && !account_name.empty()) {
                if (board_preview) show_main_section(MainSection::Dynamic);
                else show_dynamic_page();
            } else if (main_section == MainSection::Inbox && !account_name.empty()) {
                if (board_preview) show_main_section(MainSection::Inbox);
                else show_inbox_page();
            }
        }
        if (dynamic_fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            const bool current = pending_dynamic_epoch == dynamic_request_epoch &&
                                 pending_dynamic_articles == dynamic_showing_articles;
            if (!current) {
                if (main_section == MainSection::Dynamic && !dynamic_fetch_requested) {
                    if (board_preview) show_main_section(MainSection::Dynamic);
                    else show_dynamic_page();
                }
            } else if (pending_dynamic_append) {
                for (DynamicItem& item : pending_dynamic_items) {
                    const bool duplicate = !item.id.empty() && std::any_of(
                        displayed_dynamic_items.begin(), displayed_dynamic_items.end(), [&item](const DynamicItem& current) {
                            return current.id == item.id;
                        });
                    if (!duplicate && displayed_dynamic_items.size() < 32)
                        displayed_dynamic_items.push_back(std::move(item));
                }
            } else {
                displayed_dynamic_items = std::move(pending_dynamic_items);
            }
            if (current) {
                displayed_dynamic_error = std::move(pending_dynamic_error);
                if (dynamic_showing_articles && displayed_dynamic_error.empty()) ++dynamic_article_page;
                else if (!pending_dynamic_next_offset.empty()) dynamic_video_offset = std::move(pending_dynamic_next_offset);
            }
            pending_dynamic_append = false;
            if (current && main_section == MainSection::Dynamic) {
                if (board_preview) show_main_section(MainSection::Dynamic);
                else show_dynamic_page();
            }
        }
        if (dynamic_up_fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            displayed_dynamic_ups = std::move(pending_dynamic_ups);
            if (main_section == MainSection::Dynamic && !board_preview) show_dynamic_page();
        }
        if (inbox_fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            displayed_inbox_items = std::move(pending_inbox_items);
            if (main_section == MainSection::Inbox) {
                if (board_preview) show_main_section(MainSection::Inbox);
                else show_inbox_page();
            }
        }
        if (chat_fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            if (pending_chat_append) {
                for (InboxItem& item : pending_chat_messages) {
                    const bool duplicate = std::any_of(displayed_chat_messages.begin(), displayed_chat_messages.end(), [&item](const InboxItem& current) {
                        return !item.sequence.empty() && current.sequence == item.sequence;
                    });
                    if (!duplicate) displayed_chat_messages.push_back(std::move(item));
                }
            } else {
                displayed_chat_messages = std::move(pending_chat_messages);
            }
            if (!pending_chat_next_seqno.empty()) chat_next_seqno = pending_chat_next_seqno;
            pending_chat_append = false;
            if (main_section == MainSection::Inbox && inbox_active_tab == 0) show_inbox_page();
        }
        if (history_fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            displayed_history_videos = std::move(pending_history_videos);
            if (main_section == MainSection::Mine && mine_active_tab == 0) {
                if (board_preview) show_main_section(MainSection::Mine);
                else show_mine_page();
            }
        }
        if (favorite_fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            displayed_favorite_videos = std::move(pending_favorite_videos);
            if (main_section == MainSection::Mine && mine_active_tab == 1) {
                if (board_preview) show_main_section(MainSection::Mine);
                else show_mine_page();
            }
        }
        if (later_fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            displayed_later_videos = std::move(pending_later_videos);
            if (main_section == MainSection::Mine && mine_active_tab == 5) {
                if (board_preview) show_main_section(MainSection::Mine);
                else show_mine_page();
            }
        }
        if (subscription_fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            displayed_subscription_titles = std::move(pending_subscription_titles);
            if (main_section == MainSection::Mine && mine_active_tab == 2) {
                if (board_preview) show_main_section(MainSection::Mine);
                else show_mine_page();
            }
        }
        if (bangumi_fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            displayed_bangumi_titles = std::move(pending_bangumi_titles);
            if (main_section == MainSection::Mine && mine_active_tab == 3) {
                if (board_preview) show_main_section(MainSection::Mine);
                else show_mine_page();
            }
        }
        if (series_fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            displayed_series_titles = std::move(pending_series_titles);
            if (main_section == MainSection::Mine && mine_active_tab == 4) {
                if (board_preview) show_main_section(MainSection::Mine);
                else show_mine_page();
            }
        }
        if (qr_fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            if (main_section == MainSection::Mine && !qr_login_url.empty()) {
                if (content) lv_obj_del(content);
                content = lv_obj_create(lv_scr_act());
                lv_obj_set_size(content, board_preview ? 720 : 1120, board_preview ? 382 : 590);
                lv_obj_set_pos(content, board_preview ? 72 : 130, board_preview ? 86 : 110);
                lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_width(content, 0, 0);
                qr_code_widget = lv_qrcode_create(content, board_preview ? 220 : 300,
                                                   lv_color_hex(0x111111), lv_color_hex(0xffffff));
                lv_qrcode_update(qr_code_widget, qr_login_url.c_str(), qr_login_url.size());
                lv_obj_center(qr_code_widget);
                set_label(content, "Scan with Bilibili mobile app", 16, ui_color(0xf3f5f8),
                          LV_ALIGN_BOTTOM_MID, 0, -18);
                qr_status_label = lv_label_create(content);
                lv_obj_add_event_cb(qr_status_label, [](lv_event_t*) {
                    qr_status_label = nullptr;
                    qr_code_widget = nullptr;
                }, LV_EVENT_DELETE, nullptr);
                lv_obj_set_width(qr_status_label, board_preview ? 680 : 1040);
                lv_label_set_long_mode(qr_status_label, LV_LABEL_LONG_CLIP);
                lv_obj_set_style_text_font(qr_status_label, ui_font(), 0);
                lv_obj_set_style_text_color(qr_status_label, ui_color(0x8fd3ff), 0);
                lv_obj_align(qr_status_label, LV_ALIGN_BOTTOM_MID, 0, -48);
                lv_label_set_text(qr_status_label, "QR: waiting for scan");
                lv_label_set_text(status_label, "QR login code ready");
            }
        }
        if (qr_status_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            if (status_label) lv_label_set_text(status_label, pending_qr_status.c_str());
            if (qr_status_label) lv_label_set_text(qr_status_label, pending_qr_status.c_str());
        }
        if (danmaku_fetch_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            if (pending_danmaku_index < displayed_videos.size()) {
                displayed_videos[pending_danmaku_index].danmaku = std::move(pending_danmaku);
                if (player_page_visible && pending_danmaku_index == active_video_index && danmaku_label) {
                    refresh_danmaku_overlay();
                }
            }
        }
        if (action_result_complete.exchange(false)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            if (status_label) lv_label_set_text(status_label, pending_action_result.c_str());
        }
        if (refresh_chat_after_action.exchange(false) && !active_chat_talker.empty()) {
            InboxItem session;
            session.talker_id = active_chat_talker;
            session.session_type = active_chat_session_type;
            fetch_chat_messages(session);
        }
        if (refresh_detail_after_action.exchange(false) && active_video_index < displayed_videos.size()) {
            request_video_detail(active_video_index, active_part_index);
        }
        if (refresh_favorites_after_action.exchange(false)) {
            displayed_favorite_videos.clear();
            fetch_favorite_videos();
        }
        if (refresh_later_after_action.exchange(false)) {
            displayed_later_videos.clear();
            fetch_later_videos();
        }
        if (player_page_visible && player_playing && player_seek_label) {
            const uint32_t elapsed_ms = player_elapsed_offset_ms +
                (platform_ticks() - player_started_tick) * static_cast<uint32_t>(setting_playback_speed) / 100U;
            const uint32_t elapsed_sec = elapsed_ms / 1000U;
            const std::string position = std::to_string(elapsed_sec / 60U) + ":" +
                                         (elapsed_sec % 60U < 10U ? "0" : "") +
                                         std::to_string(elapsed_sec % 60U);
            set_label_text_if_changed(player_seek_label, position.c_str());
        }
        refresh_danmaku_overlay();
        const uint32_t next_timer = lv_timer_handler();
        handle_player_end();
        const uint32_t fps_now = platform_ticks();
        if (fps_label && fps_now - fps_window_start >= 1000) {
            const uint32_t elapsed = fps_now - fps_window_start;
            const uint32_t fps = elapsed ? (fps_frame_count * 1000U / elapsed) : 0;
            const std::string text = "FPS " + std::to_string(fps);
            set_label_text_if_changed(fps_label, text.c_str());
            fps_window_start = fps_now;
            fps_frame_count = 0;
        }
        const uint32_t frame_delay = setting_limited_fps > 0
            ? std::max<uint32_t>(1, 1000U / static_cast<uint32_t>(setting_limited_fps))
            : wiliwili::clamp<uint32_t>(next_timer, 5U, 20U);
        platform_delay(frame_delay);
    }
    platform_stop_text_input();
    platform_player_stop();
    lv_group_set_default(nullptr);
    if (focus_group) lv_group_del(focus_group);
    platform_font_shutdown();
    platform_display_shutdown();
    return 0;
}
