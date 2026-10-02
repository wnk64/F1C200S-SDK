#define SDL_MAIN_HANDLED
#define main wiliwili_application_main
#include "main.cpp"
#undef main

#include "bounded_http_body.h"
#include <SDL.h>
#include <SDL_image.h>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* name) {
    if (!condition) throw std::runtime_error(name);
}

void test_http_body() {
    const std::vector<uint8_t> source(70003, 0x5a);
    std::vector<uint8_t> body;
    std::string error = "previous error";
    size_t offset = 0;
    auto query = [&source, &offset](size_t& available) {
        available = std::min<size_t>(4096, source.size() - offset);
        return true;
    };
    auto read = [&source, &offset](uint8_t* destination, size_t requested, size_t& received) {
        require(requested <= 16U * 1024U, "HTTP read block exceeded limit");
        received = std::min({requested, size_t{701}, source.size() - offset});
        std::memcpy(destination, source.data() + offset, received);
        offset += received;
        return true;
    };
    require(read_bounded_http_body(query, read, body, source.size(), error) && body == source &&
            body.capacity() <= source.size() && error.empty(), "HTTP partial reads/capacity failed");
    offset = 0;
    require(!read_bounded_http_body(query, read, body, 70002, error) && body.empty(), "HTTP oversize accepted");
    offset = 0;
    auto failed_query = [&offset](size_t& available) { available = 4; return offset == 0; };
    require(!read_bounded_http_body(failed_query, read, body, 100, error) && body.empty(), "Query failure accepted as EOF");
    offset = 0;
    require(!read_bounded_http_body(query, [](uint8_t*, size_t, size_t&) { return false; }, body, source.size(), error),
            "Read failure accepted");
    require(!read_bounded_http_body(query, [](uint8_t*, size_t, size_t& received) { received = 0; return true; },
                                   body, source.size(), error), "Premature EOF accepted");
    require(!read_bounded_http_body(query, [](uint8_t*, size_t requested, size_t& received) {
        received = requested + 1; return true;
    }, body, source.size(), error), "Impossible read length accepted");
    require(read_bounded_http_body([](size_t& available) { available = 0; return true; },
                                   [](uint8_t*, size_t, size_t& received) { received = 0; return true; }, body, 0, error) &&
            body.empty() && error.empty(), "Empty HTTP semantics failed");
    bool announced = false;
    size_t read_calls = 0;
    require(!read_bounded_http_body([&announced](size_t& available) {
        available = announced ? 0 : 8;
        announced = true;
        return true;
    }, [&read_calls](uint8_t* destination, size_t requested, size_t& received) {
        received = read_calls++ == 0 ? std::min<size_t>(4, requested) : 0;
        std::memset(destination, 0x41, received);
        return true;
    }, body, 16, error) && body.empty(), "Unread announced bytes accepted as EOF");
    require(!read_bounded_http_body([](size_t& available) { available = 0; return true; },
                                   [](uint8_t*, size_t, size_t&) { return false; }, body, 16, error),
            "EOF confirmation failure accepted");
    require(!read_bounded_http_body([](size_t& available) { available = 0; return true; },
                                   [](uint8_t* destination, size_t, size_t& received) {
                                       *destination = 1; received = 1; return true;
                                   }, body, 16, error), "Data after empty availability accepted");
    std::cout << "PASS HTTP: partial reads, capacity, limits, query/read failure and EOF\n";
}

CommentPageResult comment_page(int page, size_t count) {
    CommentPageResult result;
    result.bvid = "BVoffline1";
    result.epoch = navigation_epoch.load();
    result.page = page;
    for (size_t index = 0; index < count; ++index)
        result.comments.push_back({"author", std::to_string(page) + " item " + std::to_string(index)});
    return result;
}

void test_comments() {
    const std::string unicode = u8"汉字🙂测试";
    for (size_t limit = 0; limit <= unicode.size(); ++limit) {
        std::string clipped = unicode;
        truncate_utf8_text(clipped, limit);
        require(clipped.size() <= limit && is_valid_utf8(clipped), "UTF-8 clipping split a code point");
    }
    const auto parsed = parse_video_comments(R"({"replies":[{"member":{"uname":")" + std::string(512, 'a') +
                                            R"("},"content":{"message":")" + std::string(4096, 'm') + R"("}}]})");
    require(parsed.size() == 1 && parsed[0].author.size() == kMaxCommentAuthorBytes &&
            parsed[0].message.size() == kMaxCommentMessageBytes, "Comment text budget failed");
    displayed_videos.assign(1, {});
    displayed_videos[0].bvid = "BVoffline1";
    active_video_index = 0;
    navigation_epoch = 77;
    comment_next_page = 2;
    auto first = comment_page(2, 8);
    require(merge_comment_page(first) && displayed_videos[0].comments.size() == 8, "First comment page failed");
    auto stale = comment_page(3, 8);
    stale.epoch = 76;
    require(!merge_comment_page(stale), "Stale epoch accepted");
    stale = comment_page(3, 8);
    stale.bvid = "BVreplaced";
    require(!merge_comment_page(stale), "Replaced video received old comments");
    stale = comment_page(3, 8);
    stale.video_index = 1;
    require(!merge_comment_page(stale), "Invalid index accepted");
    stale = comment_page(4, 8);
    require(!merge_comment_page(stale), "Wrong page accepted");
    stale = comment_page(3, 8);
    stale.error = "offline failure";
    require(!merge_comment_page(stale) && comment_next_page == 3, "Failed page advanced cursor");
    auto remainder = comment_page(3, 100);
    require(merge_comment_page(remainder) && displayed_videos[0].comments.size() == kMaxVideoComments &&
            displayed_videos[0].comments_exhausted, "Comment cap failed");
    auto extra = comment_page(4, 8);
    require(merge_comment_page(extra) && displayed_videos[0].comments.size() == kMaxVideoComments, "Comment cap grew");
    displayed_videos[0].comments.clear();
    displayed_videos[0].comments_exhausted = false;
    auto empty = comment_page(comment_next_page, 0);
    require(merge_comment_page(empty) && displayed_videos[0].comments_exhausted, "Empty page kept loading");
    std::cout << "PASS comments: UTF-8/text/count caps, stale identity, failed/empty pages\n";
}

void test_labels() {
    lv_init();
    static std::array<lv_color_t, 800> pixels{};
    static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer, pixels.data(), nullptr, pixels.size());
    lv_disp_drv_t driver;
    lv_disp_drv_init(&driver);
    driver.hor_res = 800;
    driver.ver_res = 480;
    driver.draw_buf = &buffer;
    driver.flush_cb = [](lv_disp_drv_t* display, const lv_area_t*, lv_color_t*) { lv_disp_flush_ready(display); };
    lv_disp_t* display = lv_disp_drv_register(&driver);
    lv_obj_t* label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "0:01");
    lv_mem_monitor_t before{}, after{};
    lv_mem_monitor(&before);
    for (size_t count = 0; count < 10000; ++count)
        require(!set_label_text_if_changed(label, "0:01"), "Unchanged label updated");
    lv_mem_monitor(&after);
    require(before.free_size == after.free_size && before.frag_pct == after.frag_pct, "Unchanged labels churned heap");
    require(set_label_text_if_changed(label, "0:02") && !set_label_text_if_changed(nullptr, "text"), "Label update failed");
    setting_app_theme = "dark";
    require(lv_color_to32(ui_color(0x111827)) == lv_color_to32(lv_color_hex(0x111827)), "Dark palette changed");
    setting_app_theme = "light";
    require(lv_color_to32(ui_color(0x111827)) == lv_color_to32(lv_color_hex(0xf8f9fb)) &&
            lv_color_to32(ui_color(0xffffff)) == lv_color_to32(lv_color_hex(0x18212c)), "Native light palette failed");
    setting_app_theme = "auto";
    lv_disp_remove(display);
    std::cout << "PASS labels: 10000 unchanged updates, changed text and native palettes\n";
}

std::vector<uint8_t> png_fixture(bool alpha) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 2, 2, 32, SDL_PIXELFORMAT_ARGB8888);
    require(surface != nullptr, "PNG fixture allocation failed");
    SDL_FillRect(surface, nullptr, 0xff804020U);
    if (alpha) {
        auto* pixels = static_cast<uint32_t*>(surface->pixels);
        pixels[0] = 0x00804020U;
        pixels[1] = 0x7f804020U;
    }
    std::vector<uint8_t> encoded(4096);
    SDL_RWops* stream = SDL_RWFromMem(encoded.data(), static_cast<int>(encoded.size()));
    const int saved = IMG_SavePNG_RW(surface, stream, 0);
    const Sint64 length = SDL_RWtell(stream);
    SDL_RWclose(stream);
    SDL_FreeSurface(surface);
    require(saved == 0 && length > 0 && static_cast<size_t>(length) <= encoded.size(), "PNG encoding failed");
    encoded.resize(static_cast<size_t>(length));
    return encoded;
}

void test_thumbnails() {
    IMG_Init(IMG_INIT_PNG);
    std::vector<uint8_t> pixels;
    bool has_alpha = true;
    auto encoded = png_fixture(false);
    require(platform_decode_thumbnail_lvgl(encoded, 2, 2, pixels, &has_alpha) && !has_alpha &&
            pixels.size() == 4 * sizeof(lv_color_t), "Opaque thumbnail layout failed");
    encoded = png_fixture(true);
    require(platform_decode_thumbnail_lvgl(encoded, 2, 2, pixels, &has_alpha) && has_alpha &&
            pixels.size() == 4 * LV_IMG_PX_SIZE_ALPHA_BYTE && pixels[LV_IMG_PX_SIZE_ALPHA_BYTE - 1] == 0 &&
            pixels[2 * LV_IMG_PX_SIZE_ALPHA_BYTE - 1] == 127, "Transparent alpha was lost");
    encoded[16] = encoded[17] = encoded[19] = encoded[20] = encoded[21] = encoded[23] = 0;
    encoded[18] = encoded[22] = 8;
    require(!platform_decode_thumbnail_lvgl(encoded, 2, 2, pixels, &has_alpha) && pixels.empty(), "Oversized PNG decoded");
    IMG_Quit();
    std::cout << "PASS thumbnails: opaque/transparent layout, size guard (depth " << LV_COLOR_DEPTH << ")\n";
}

}

int main() {
    try {
        test_http_body();
        test_comments();
        test_labels();
        test_thumbnails();
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "FAIL " << failure.what() << '\n';
        return 1;
    }
}
