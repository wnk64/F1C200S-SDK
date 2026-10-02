#pragma once
#include <cstdint>

namespace board {
constexpr int width = 800, height = 480;
constexpr int panel_width = 480, panel_height = 800;
inline bool rectangle(int x, int y, int w, int h) {
    return x >= 0 && y >= 0 && x < width && y < height &&
           w > 0 && h > 0 && w <= width - x && h <= height - y;
}
inline void to_panel(int x, int y, int& px, int& py) { px = height - 1 - y; py = x; }
inline void from_panel(int px, int py, int& x, int& y) { x = py; y = height - 1 - px; }
inline uint32_t rgb565(uint16_t value) {
    unsigned r = (value >> 11) & 31, g = (value >> 5) & 63, b = value & 31;
    return (((r << 3) | (r >> 2)) << 16) | (((g << 2) | (g >> 4)) << 8) | ((b << 3) | (b >> 2));
}
}
