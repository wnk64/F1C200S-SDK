#define SDL_MAIN_HANDLED
#include <SDL.h>

extern "C" {
#include "lvgl.h"
}

#include <cstdint>
#include <fstream>
#include <vector>

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_Texture* texture = nullptr;
std::vector<uint32_t> frame(kWindowWidth * kWindowHeight);

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

void add_label(lv_obj_t* parent, const char* text, const lv_font_t* font,
               lv_color_t color, lv_align_t align, int x, int y) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_align(label, align, x, y);
}

void build_source_migration_notice() {
    lv_obj_t* root = lv_scr_act();
    lv_obj_set_style_bg_color(root, lv_color_hex(0x181c25), 0);

    lv_obj_t* panel = lv_obj_create(root);
    lv_obj_set_size(panel, 980, 420);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x222936), 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_border_width(panel, 0, 0);

    add_label(panel, "wiliwili LVGL rewrite", &lv_font_montserrat_24,
              lv_color_hex(0xffffff), LV_ALIGN_TOP_LEFT, 42, 42);
    add_label(panel, "Windows source-migration worktree", &lv_font_montserrat_16,
              lv_color_hex(0x68b7f0), LV_ALIGN_TOP_LEFT, 42, 84);
    add_label(panel,
              "The original Borealis/NanoVG activities, fragments, presenters,"
              " API models and resources are the migration contract.",
              &lv_font_montserrat_16, lv_color_hex(0xc7d2de), LV_ALIGN_TOP_LEFT, 42, 148);
    add_label(panel,
              "This executable is only the Windows LVGL display bootstrap."
              " Page-by-page parity starts from main.xml and HomeTab.",
              &lv_font_montserrat_16, lv_color_hex(0xc7d2de), LV_ALIGN_TOP_LEFT, 42, 212);
    add_label(panel, "Esc closes the test window.", &lv_font_montserrat_14,
              lv_color_hex(0x91a2b4), LV_ALIGN_BOTTOM_LEFT, 42, -34);
}

}  // namespace

int main() {
    std::ofstream trace("E:/LVGL界面/wiliwili_lvgl/windows_lvgl_bootstrap.log", std::ios::app);
    trace << "main\n" << std::flush;
    SDL_SetMainReady();
    trace << "main-ready\n" << std::flush;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) return 1;
    trace << "sdl-init\n" << std::flush;
    window = SDL_CreateWindow("wiliwili LVGL", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, kWindowWidth, kWindowHeight, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, kWindowWidth, kWindowHeight);
    if (!window || !renderer || !texture) return 1;
    trace << "window-ready\n" << std::flush;

    lv_init();
    trace << "lv-init\n" << std::flush;
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
    build_source_migration_notice();
    trace << "ui-ready\n" << std::flush;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                running = false;
            }
        }
        lv_tick_inc(16);
        lv_timer_handler();
        SDL_Delay(16);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
