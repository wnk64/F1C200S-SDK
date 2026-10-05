#include "engine_runtime.h"
extern "C" {
#include "platform.h"
}
#include <cassert>
#include <cstdio>
int main()
{
    assert(!ons_rt_Init(ons_rt_INIT_VIDEO));
    ons_surface *screen = ons_rt_SetVideoMode(640, 480, 32, 0); assert(screen);
    lv_obj_t *bar = lv_obj_get_child(lv_scr_act(), 1); assert(bar);
    lv_obj_update_layout(bar);
    lv_area_t bounds; lv_obj_get_coords(bar, &bounds);
    assert(bounds.y1 >= 720 && bounds.y2 <= 799 && lv_obj_get_child_cnt(bar) == 6);
    const int keys[] = {ONSK_ESCAPE, ONSK_LEFT, ONSK_a, ONSK_s, ONSK_0};
    ons_rt_Event events[128];
    for (unsigned i = 0; i < 6; ++i) {
        lv_obj_t *button = lv_obj_get_child(bar, i); lv_obj_get_coords(button, &bounds);
        assert(bounds.y1 >= 720 && bounds.y2 <= 799 && bounds.x1 >= 0 && bounds.x2 <= 479);
        assert(!lv_obj_get_style_shadow_width(button, LV_PART_MAIN));
        assert(lv_event_send(button, LV_EVENT_CLICKED, NULL) == LV_RES_OK);
        int count = ons_rt_PeepEvents(events, 128, ons_rt_GETEVENT, ons_rt_ALLEVENTS);
        if (i < 5) {
            assert(count == 2 && events[0].type == ons_rt_KEYDOWN && events[1].type == ons_rt_KEYUP);
            assert(events[0].key.keysym.sym == keys[i] && events[1].key.keysym.sym == keys[i]);
        } else assert(count == 1 && events[0].type == ons_rt_QUIT);
    }
    ons_rt_Quit();
    std::puts("Native control geometry/key-pairs/exit PASS");
    return 0;
}
