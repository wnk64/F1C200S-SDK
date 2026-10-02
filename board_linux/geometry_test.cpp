#include "geometry.h"
#include "compat_cpp17.h"
#include <cassert>
#include <climits>
#include <iostream>
int main() {
    for (int y = 0; y < board::height; ++y) for (int x = 0; x < board::width; ++x) {
        int px, py, rx, ry; board::to_panel(x, y, px, py); board::from_panel(px, py, rx, ry);
        assert(px >= 0 && px < 480 && py >= 0 && py < 800 && rx == x && ry == y);
    }
    assert(board::rectangle(0, 0, 800, 480));
    assert(board::rectangle(799, 479, 1, 1));
    assert(!board::rectangle(-1, 0, 1, 1));
    assert(!board::rectangle(0, 0, INT_MAX, INT_MAX));
    assert(!board::rectangle(INT_MAX, INT_MAX, 1, 1));
    assert(!board::rectangle(799, 479, 2, 1));
    assert(board::rgb565(0xffff) == 0xffffff);
    assert(board::rgb565(0xf800) == 0xff0000);
    assert(board::rgb565(0x07e0) == 0x00ff00);
    assert(board::rgb565(0x001f) == 0x0000ff);
    assert(wiliwili::clamp(20, 0, 10) == 10);
    wiliwili::string_view v("hello"); assert(v.substr(1, 3) == "ell");
    std::cout << "384000 coordinate round trips, bounds, RGB565 and GCC compatibility passed\n";
}
