#include "engine_runtime.h"
#include <cassert>
#include <cstdio>
static uint32_t timer(uint32_t, void *)
{
    ons_rt_Event event = {}; event.type = ons_rt_USEREVENT; event.user.code = 42;
    assert(!ons_rt_PushEvent(&event)); return 0;
}
int main()
{
    assert(!ons_rt_Init(ons_rt_INIT_TIMER));
    ons_rt_Event first = {}, second = {}, received;
    first.type = ons_rt_KEYDOWN; first.key.keysym.sym = ONSK_RETURN;
    second.type = ons_rt_MOUSEBUTTONUP; second.button.button = ons_rt_BUTTON_LEFT;
    assert(!ons_rt_PushEvent(&first) && !ons_rt_PushEvent(&second));
    assert(ons_rt_PeepEvents(&received, 1, ons_rt_PEEKEVENT, ons_rt_EVENTMASK(ons_rt_MOUSEBUTTONUP)) == 1 && received.type == second.type);
    assert(ons_rt_PollEvent(&received) == 1 && received.type == first.type);
    assert(ons_rt_PollEvent(&received) == 1 && received.type == second.type);
    assert(ons_rt_PollEvent(&received) == 0);
    ons_rt_TimerID handle = ons_rt_AddTimer(20, timer, NULL); assert(handle);
    assert(ons_rt_WaitEvent(&received) == 1 && received.type == ons_rt_USEREVENT && received.user.code == 42);
    ons_rt_RemoveTimer(handle);
    handle = ons_rt_AddTimer(100, timer, NULL); assert(handle && ons_rt_RemoveTimer(handle));
    ons_rt_Delay(150); assert(ons_rt_PollEvent(&received) == 0);
    assert(ons_rt_InitSubSystem(ons_rt_INIT_JOYSTICK) == -1);
    ons_rt_Quit();
    std::puts("Native execution event mask/order/timer thread/cancel/quit tests PASS"); return 0;
}
