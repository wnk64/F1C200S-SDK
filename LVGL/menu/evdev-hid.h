#ifndef F1_EVDEV_HID_H
#define F1_EVDEV_HID_H
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

enum { F1_HID_DEVICES = 8, F1_HID_EVENTS = 64,
       F1_UP = 0x100, F1_DOWN, F1_LEFT, F1_RIGHT, F1_HOME, F1_END, F1_NEXT, F1_PREV };
typedef struct { unsigned key; int down; } f1_hid_key;
typedef struct { int x, y, down; } f1_hid_point;
typedef struct {
    int fd, number, mouse, keyboard, gamepad, left, shift, caps;
    uint16_t buttons, axes;
    int axis_min[4], axis_max[4], axis_direction[2], hat_direction[2];
    dev_t dev; ino_t ino;
    unsigned mapped[128];
} f1_hid_device;
typedef struct {
    f1_hid_device devices[F1_HID_DEVICES];
    f1_hid_key keys[F1_HID_EVENTS];
    f1_hid_point points[F1_HID_EVENTS];
    unsigned kh, kn, ph, pn, overflows;
    int x, y, width, height, mouse_count;
    uint64_t next_scan;
} f1_hid;
static void f1_key_push(f1_hid *, unsigned, int);
static inline uint16_t f1_hid_pad(const f1_hid *s)
{
    uint16_t pad = 0;
    for(int i=0;i<F1_HID_DEVICES;i++)pad |= s->devices[i].buttons | s->devices[i].axes;
    return pad;
}
static int f1_pad_button(unsigned code)
{
    switch(code){
    case BTN_SOUTH:return 0;case BTN_EAST:return 8;case BTN_NORTH:return 9;case BTN_WEST:return 1;
    case BTN_SELECT:return 2;case BTN_START:return 3;case BTN_TL:return 10;case BTN_TR:return 11;
    case BTN_TL2:return 12;case BTN_TR2:return 13;case BTN_THUMBL:return 14;case BTN_THUMBR:return 15;
    case BTN_DPAD_UP:return 4;case BTN_DPAD_DOWN:return 5;case BTN_DPAD_LEFT:return 6;case BTN_DPAD_RIGHT:return 7;
    default:return -1;
    }
}
static void f1_pad_axes(f1_hid *s,f1_hid_device *d)
{
    uint16_t before=d->axes;
    d->axes=0;
    for(int a=0;a<2;a++){
        int direction=d->hat_direction[a]?d->hat_direction[a]:d->axis_direction[a];
        if(direction)d->axes |= 1u<<(a==0?(direction<0?6:7):(direction<0?4:5));
    }
    unsigned prior=before&((1u<<4)|(1u<<6)),next=d->axes&((1u<<4)|(1u<<6));
    if(prior&&!next)f1_key_push(s,F1_PREV,0);
    if(next&&!prior)f1_key_push(s,F1_PREV,1);
    prior=before&((1u<<5)|(1u<<7));next=d->axes&((1u<<5)|(1u<<7));
    if(prior&&!next)f1_key_push(s,F1_NEXT,0);
    if(next&&!prior)f1_key_push(s,F1_NEXT,1);
}
#define F1_BITS(n) (((n) + 8 * sizeof(unsigned long)) / (8 * sizeof(unsigned long)))
static int f1_bit(const unsigned long *bits, unsigned bit)
{ return (bits[bit / (8 * sizeof(unsigned long))] >> (bit % (8 * sizeof(unsigned long)))) & 1; }
static int f1_capabilities(int fd, int *mouse, int *keyboard, int *touch)
{
    unsigned long keys[F1_BITS(KEY_MAX)] = {0}, rel[F1_BITS(REL_MAX)] = {0}, abs[F1_BITS(ABS_MAX)] = {0};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0) return -1;
    (void)ioctl(fd, EVIOCGBIT(EV_REL, sizeof(rel)), rel);
    (void)ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs)), abs);
    *touch = f1_bit(keys, BTN_TOUCH) && (f1_bit(abs, ABS_X) || f1_bit(abs, ABS_MT_POSITION_X));
    *mouse = !*touch && f1_bit(keys, BTN_LEFT) && f1_bit(rel, REL_X) && f1_bit(rel, REL_Y);
    *keyboard = !*touch && f1_bit(keys, KEY_ENTER) && f1_bit(keys, KEY_A);
    return 0;
}
static unsigned f1_key(unsigned code, int shift, int caps)
{
    static const char letters[] = "qwertyuiopasdfghjklzxcvbnm";
    static const unsigned codes[] = {KEY_Q,KEY_W,KEY_E,KEY_R,KEY_T,KEY_Y,KEY_U,KEY_I,KEY_O,KEY_P,
        KEY_A,KEY_S,KEY_D,KEY_F,KEY_G,KEY_H,KEY_J,KEY_K,KEY_L,KEY_Z,KEY_X,KEY_C,KEY_V,KEY_B,KEY_N,KEY_M};
    for (unsigned i = 0; i < sizeof(codes)/sizeof(*codes); i++)
        if (code == codes[i]) return (shift != caps) ? letters[i] - 'a' + 'A' : letters[i];
    if (code >= KEY_1 && code <= KEY_0) return shift ? "!@#$%^&*()"[code-KEY_1] : "1234567890"[code-KEY_1];
    switch (code) {
    case KEY_TAB: return shift ? F1_PREV : F1_NEXT;
    case KEY_ENTER: case KEY_KPENTER: return '\n';
    case KEY_ESC: return 27;
    case KEY_BACKSPACE: return 8;
    case KEY_DELETE: return 127;
    case KEY_UP: return F1_UP; case KEY_DOWN: return F1_DOWN;
    case KEY_LEFT: return F1_LEFT; case KEY_RIGHT: return F1_RIGHT;
    case KEY_HOME: return F1_HOME; case KEY_END: return F1_END;
    case KEY_SPACE: return ' ';
    case KEY_MINUS: return shift ? '_' : '-'; case KEY_EQUAL: return shift ? '+' : '=';
    case KEY_LEFTBRACE: return shift ? '{' : '['; case KEY_RIGHTBRACE: return shift ? '}' : ']';
    case KEY_SEMICOLON: return shift ? ':' : ';'; case KEY_APOSTROPHE: return shift ? '"' : '\'';
    case KEY_GRAVE: return shift ? '~' : '`'; case KEY_BACKSLASH: return shift ? '|' : '\\';
    case KEY_COMMA: return shift ? '<' : ','; case KEY_DOT: return shift ? '>' : '.';
    case KEY_SLASH: return shift ? '?' : '/';
    default: return 0;
    }
}
static void f1_key_push(f1_hid *s, unsigned key, int down)
{
    if (!key) return;
    if (s->kn == F1_HID_EVENTS) { s->kh = s->kn = 0; s->overflows++; }
    s->keys[(s->kh + s->kn++) % F1_HID_EVENTS] = (f1_hid_key){key, down};
}
static void f1_point_push(f1_hid *s)
{
    int down = 0;
    for (int i = 0; i < F1_HID_DEVICES; i++) down |= s->devices[i].left;
    if (s->pn == F1_HID_EVENTS) { s->ph = s->pn = 0; s->overflows++; }
    s->points[(s->ph + s->pn++) % F1_HID_EVENTS] = (f1_hid_point){s->x, s->y, down};
}
static void f1_hid_event(f1_hid *s, f1_hid_device *d, const struct input_event *e)
{
    if(d->gamepad&&e->type==EV_KEY){
        int bit=f1_pad_button(e->code);
        if(bit>=0){if(e->value)d->buttons|=1u<<bit;else d->buttons&=~(1u<<bit);}
        unsigned key=e->code==BTN_SOUTH||e->code==BTN_START?'\n':e->code==BTN_EAST||e->code==BTN_MODE?27:0;
        if(bit==4||bit==6)key=F1_PREV;
        if(bit==5||bit==7)key=F1_NEXT;
        if(key)f1_key_push(s,key,e->value!=0);
    }
    if(d->gamepad&&e->type==EV_ABS){
        int trigger=e->code==ABS_Z?2:e->code==ABS_RZ?3:-1;
        if(trigger>=0){int64_t range=(int64_t)d->axis_max[trigger]-d->axis_min[trigger];
            unsigned bit=trigger==2?12:13;
            if(range>0&&e->value>(int64_t)d->axis_min[trigger]+range/4)d->buttons|=1u<<bit;
            else d->buttons&=~(1u<<bit);}
        int axis=e->code==ABS_X?0:e->code==ABS_Y?1:-1;
        if(axis>=0){int64_t span=(int64_t)d->axis_max[axis]-d->axis_min[axis];
            int64_t center=(int64_t)d->axis_min[axis]+span/2;
            d->axis_direction[axis]=span>0?(e->value<center-span/4?-1:e->value>center+span/4?1:0):0;}
        if(e->code==ABS_HAT0X||e->code==ABS_HAT0Y)d->hat_direction[e->code==ABS_HAT0X?0:1]=e->value<0?-1:e->value>0?1:0;
        f1_pad_axes(s,d);
    }
    if (d->mouse && e->type == EV_REL) {
        /* Clamp the delta before addition to avoid overflow on malformed events. */
        int limit = e->code == REL_X ? s->width : s->height;
        int delta = e->value > limit ? limit : e->value < -limit ? -limit : e->value;
        if (e->code == REL_X) s->x += delta;
        if (e->code == REL_Y) s->y += delta;
        if (s->x < 0) s->x = 0;
        if (s->x >= s->width) s->x = s->width - 1;
        if (s->y < 0) s->y = 0;
        if (s->y >= s->height) s->y = s->height - 1;
        if (e->code == REL_WHEEL && e->value) {
            unsigned key = e->value > 0 ? F1_PREV : F1_NEXT;
            f1_key_push(s, key, 1); f1_key_push(s, key, 0);
        }
    }
    if (d->mouse && e->type == EV_KEY && e->code == BTN_LEFT) d->left = e->value != 0;
    if (d->mouse && e->type == EV_KEY && e->code == BTN_RIGHT) f1_key_push(s, 27, e->value != 0);
    if (d->mouse && e->type == EV_SYN && e->code == SYN_REPORT) f1_point_push(s);
    if (!d->keyboard || e->type != EV_KEY) return;
    if (e->code == KEY_LEFTSHIFT || e->code == KEY_RIGHTSHIFT) {
        int bit = e->code == KEY_LEFTSHIFT ? 1 : 2;
        if (e->value) d->shift |= bit; else d->shift &= ~bit;
        return;
    }
    if (e->code == KEY_CAPSLOCK) { if (e->value == 1) d->caps = !d->caps; return; }
    if (e->code >= 128) return;
    unsigned key = d->mapped[e->code];
    if (e->value && !key) key = f1_key(e->code, d->shift != 0, d->caps);
    if (e->value) d->mapped[e->code] = key; else d->mapped[e->code] = 0;
    f1_key_push(s, key, e->value != 0);
}
static void f1_hid_drop(f1_hid *s, f1_hid_device *d)
{
    if(d->gamepad){f1_key_push(s,F1_PREV,0);f1_key_push(s,F1_NEXT,0);f1_key_push(s,'\n',0);f1_key_push(s,27,0);}
    for (int key = 0; key < 128; key++) if (d->mapped[key]) f1_key_push(s, d->mapped[key], 0);
    if (d->fd >= 0) close(d->fd);
    int mouse = d->mouse;
    memset(d, 0, sizeof(*d)); d->fd = -1;
    if (mouse) { s->mouse_count--; f1_point_push(s); }
}
static void f1_hid_init(f1_hid *s)
{
    memset(s, 0, sizeof(*s)); s->width = 480; s->height = 800; s->x = 240; s->y = 400;
    for (int i = 0; i < F1_HID_DEVICES; i++) s->devices[i].fd = -1;
}
static void f1_hid_scan(f1_hid *s, uint64_t now)
{
    if (now < s->next_scan) return;
    s->next_scan = now + 1000;
    for (int number = 0; number < 64; number++) {
        char path[64]; struct stat st; snprintf(path, sizeof(path), "/dev/input/event%d", number);
        int exists = stat(path, &st) == 0, found = 0, empty = -1;
        for (int i = 0; i < F1_HID_DEVICES; i++) {
            f1_hid_device *d = &s->devices[i];
            if (d->fd >= 0 && d->number == number) {
                if (exists && d->dev == st.st_rdev && d->ino == st.st_ino) found = 1;
                else f1_hid_drop(s, d);
            }
            if (d->fd < 0) empty = i;
        }
        if (!exists || found || empty < 0) continue;
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC), mouse, keyboard, touch;
        if (fd < 0) continue;
        unsigned long pad_keys[F1_BITS(KEY_MAX)]={0};
        int gamepad=ioctl(fd,EVIOCGBIT(EV_KEY,sizeof(pad_keys)),pad_keys)>=0&&f1_bit(pad_keys,BTN_GAMEPAD);
        if (f1_capabilities(fd, &mouse, &keyboard, &touch) || (!mouse && !keyboard&&!gamepad)) { close(fd); continue; }
        f1_hid_device *d = &s->devices[empty]; d->fd = fd; d->number = number;
        d->mouse = mouse; d->keyboard = keyboard; d->dev = st.st_rdev; d->ino = st.st_ino;
        d->gamepad=gamepad;
        const unsigned axis_codes[]={ABS_X,ABS_Y,ABS_Z,ABS_RZ};
        for(int a=0;a<4;a++){struct input_absinfo info;
            if(gamepad&&ioctl(fd,EVIOCGABS(axis_codes[a]),&info)==0){d->axis_min[a]=info.minimum;d->axis_max[a]=info.maximum;}}
        s->mouse_count += mouse;
    }
}
static void f1_hid_poll(f1_hid *s, uint64_t now, int width, int height)
{
    s->width = width; s->height = height; f1_hid_scan(s, now);
    for (int i = 0; i < F1_HID_DEVICES; i++) {
        f1_hid_device *d = &s->devices[i];
        if (d->fd < 0) continue;
        for (int budget = 0; budget < 64; budget++) {
            if (s->kn >= F1_HID_EVENTS - 2 || s->pn >= F1_HID_EVENTS - 2) return;
            struct input_event event; ssize_t n = read(d->fd, &event, sizeof(event));
            if (n < 0 && (errno == EAGAIN || errno == EINTR)) break;
            if (n != sizeof(event) || (event.type == EV_SYN && event.code == SYN_DROPPED)) { f1_hid_drop(s, d); break; }
            f1_hid_event(s, d, &event);
            if (s->kn >= F1_HID_EVENTS - 2 || s->pn >= F1_HID_EVENTS - 2) break;
        }
    }
}
static int f1_touch_open(void)
{
    for (int number = 0; number < 64; number++) {
        char path[64]; snprintf(path, sizeof(path), "/dev/input/event%d", number);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC), mouse, keyboard, touch;
        if (fd < 0) continue;
        if (!f1_capabilities(fd, &mouse, &keyboard, &touch) && touch) return fd;
        close(fd);
    }
    return -1;
}
static void f1_hid_close(f1_hid *s)
{ for (int i = 0; i < F1_HID_DEVICES; i++) if (s->devices[i].fd >= 0) f1_hid_drop(s, &s->devices[i]); }
#endif
