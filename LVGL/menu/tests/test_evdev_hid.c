#include <assert.h>
#include <limits.h>
#include "evdev-hid.h"
static void event(f1_hid *s, f1_hid_device *d, unsigned type, unsigned code, int value)
{
    struct input_event e = {.type = type, .code = code, .value = value};
    f1_hid_event(s, d, &e);
}
int main(void)
{
    f1_hid s; f1_hid_init(&s);
    f1_hid_device *d = &s.devices[0]; d->mouse = d->keyboard = 1;
    event(&s,d,EV_REL,REL_X,INT_MAX); event(&s,d,EV_REL,REL_Y,INT_MIN);
    event(&s,d,EV_SYN,SYN_REPORT,0);
    assert(s.points[0].x == 479 && s.points[0].y == 0);
    event(&s,d,EV_KEY,BTN_LEFT,1); event(&s,d,EV_SYN,SYN_REPORT,0);
    event(&s,d,EV_KEY,BTN_LEFT,0); event(&s,d,EV_SYN,SYN_REPORT,0);
    assert(s.pn == 3 && s.points[1].down == 1 && s.points[2].down == 0);
    event(&s,d,EV_KEY,KEY_LEFTSHIFT,1); event(&s,d,EV_KEY,KEY_A,1);
    event(&s,d,EV_KEY,KEY_LEFTSHIFT,0); event(&s,d,EV_KEY,KEY_A,2); event(&s,d,EV_KEY,KEY_A,0);
    assert(s.keys[0].key == 'A' && s.keys[1].key == 'A' && s.keys[2].key == 'A' && !s.keys[2].down);
    event(&s,d,EV_KEY,KEY_CAPSLOCK,1); event(&s,d,EV_KEY,KEY_B,1); event(&s,d,EV_KEY,KEY_B,0);
    assert(s.keys[3].key == 'B');
    event(&s,d,EV_KEY,KEY_LEFTSHIFT,1); event(&s,d,EV_KEY,KEY_C,1); event(&s,d,EV_KEY,KEY_C,0);
    assert(s.keys[5].key == 'c');
    event(&s,d,EV_KEY,KEY_RIGHTSHIFT,1); event(&s,d,EV_KEY,KEY_LEFTSHIFT,0);
    event(&s,d,EV_KEY,KEY_TAB,1); event(&s,d,EV_KEY,KEY_TAB,0);
    assert(s.keys[7].key == F1_PREV);
    event(&s,d,EV_KEY,KEY_RIGHTSHIFT,0);
    event(&s,d,EV_KEY,KEY_TAB,1); event(&s,d,EV_KEY,KEY_TAB,0);
    assert(s.keys[9].key == F1_NEXT);
    assert(f1_key(KEY_1,1,0) == '!' && f1_key(KEY_0,0,0) == '0');
    assert(f1_key(KEY_SLASH,1,0) == '?' && f1_key(KEY_ENTER,0,0) == '\n');
    assert(f1_key(KEY_UP,0,0) == F1_UP && f1_key(KEY_F1,0,0) == 0);
    event(&s,d,EV_KEY,KEY_D,1); event(&s,d,EV_KEY,BTN_LEFT,1);
    unsigned before = s.kn; s.mouse_count = 1;
    f1_hid_drop(&s,d);
    assert(s.kn == before + 1 && !s.keys[s.kn-1].down && !s.points[s.pn-1].down && s.mouse_count == 0);
    assert(d->fd == -1);
    for (int n = 0; n < 1000; n++) f1_key_push(&s,'x',n%2);
    assert(s.kn <= F1_HID_EVENTS && s.overflows > 0);
    for (int n = 0; n < 1000; n++) f1_point_push(&s);
    assert(s.pn <= F1_HID_EVENTS);
    f1_hid_close(&s);
    f1_hid_init(&s);d=&s.devices[0];d->gamepad=1;
    d->axis_min[0]=d->axis_min[1]=0;d->axis_max[0]=d->axis_max[1]=65535;
    event(&s,d,EV_KEY,BTN_SOUTH,1);assert(f1_hid_pad(&s)==1&&s.keys[0].key=='\n');
    event(&s,d,EV_ABS,ABS_X,0);assert(f1_hid_pad(&s)&(1u<<6));
    event(&s,d,EV_ABS,ABS_Y,65535);assert(f1_hid_pad(&s)&(1u<<5));
    event(&s,d,EV_ABS,ABS_X,32768);assert(!(f1_hid_pad(&s)&(1u<<6)));
    event(&s,d,EV_ABS,ABS_HAT0X,-1);assert(f1_hid_pad(&s)&(1u<<6));
    event(&s,d,EV_KEY,BTN_TR,1);assert(f1_hid_pad(&s)&(1u<<11));
    d->axis_min[2]=0;d->axis_max[2]=1023;
    event(&s,d,EV_ABS,ABS_Z,900);assert(f1_hid_pad(&s)&(1u<<12));
    event(&s,d,EV_ABS,ABS_Z,0);assert(!(f1_hid_pad(&s)&(1u<<12)));
    f1_hid_drop(&s,d);assert(!f1_hid_pad(&s));
    puts("evdev HID mapping/rapid click/modifiers/disconnect/queue bounds PASS");
    return 0;
}
