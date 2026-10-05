#ifndef ONS_ENGINE_RUNTIME_H
#define ONS_ENGINE_RUNTIME_H
#include "engine_types.h"
#include "font.h"
#include <stddef.h>

/* Native contracts used by the mechanically migrated original execution layer. */
typedef int ons_rt_Key;
enum {
    ONSK_UNKNOWN=0, ONSK_BACKSPACE=8, ONSK_TAB=9, ONSK_RETURN=13, ONSK_ESCAPE=27, ONSK_SPACE=32,
    ONSK_MINUS=45, ONSK_0=48, ONSK_1, ONSK_2, ONSK_3, ONSK_4, ONSK_5, ONSK_6, ONSK_7, ONSK_8, ONSK_9,
    ONSK_a=97, ONSK_c=99, ONSK_d=100, ONSK_f=102, ONSK_h=104, ONSK_j=106, ONSK_k=107,
    ONSK_l=108, ONSK_m=109, ONSK_n=110, ONSK_o=111, ONSK_p=112, ONSK_q=113, ONSK_r=114,
    ONSK_s=115, ONSK_v=118, ONSK_w=119, ONSK_x=120, ONSK_z=122,
    ONSK_KP0=256, ONSK_KP1, ONSK_KP2, ONSK_KP3, ONSK_KP4, ONSK_KP5, ONSK_KP6, ONSK_KP7, ONSK_KP8, ONSK_KP9,
    ONSK_KP_ENTER=271, ONSK_KP_MINUS=269, ONSK_UP=273, ONSK_DOWN, ONSK_RIGHT, ONSK_LEFT,
    ONSK_INSERT=277, ONSK_PAGEUP=280, ONSK_PAGEDOWN, ONSK_F1=282, ONSK_F12=293,
    ONSK_RSHIFT=303, ONSK_LSHIFT, ONSK_RCTRL, ONSK_LCTRL
};
enum {
    ons_rt_ACTIVEEVENT=1, ons_rt_KEYDOWN=2, ons_rt_KEYUP=3, ons_rt_MOUSEMOTION=4,
    ons_rt_MOUSEBUTTONDOWN=5, ons_rt_MOUSEBUTTONUP=6, ons_rt_JOYAXISMOTION=7,
    ons_rt_JOYBUTTONDOWN=10, ons_rt_JOYBUTTONUP=11, ons_rt_QUIT=12,
    ons_rt_VIDEOEXPOSE=17, ons_rt_USEREVENT=24,
    ons_rt_BUTTON_LEFT=1, ons_rt_BUTTON_MIDDLE=2, ons_rt_BUTTON_RIGHT=3,
    ons_rt_BUTTON_WHEELUP=4, ons_rt_BUTTON_WHEELDOWN=5,
    ons_rt_APPMOUSEFOCUS=1, ons_rt_APPACTIVE=4,
    ons_rt_PEEKEVENT=1, ons_rt_GETEVENT=2,
    ons_rt_INIT_TIMER=1, ons_rt_INIT_AUDIO=16, ons_rt_INIT_VIDEO=32,
    ons_rt_INIT_CDROM=256, ons_rt_INIT_JOYSTICK=512,
    ons_rt_DISABLE=0, ons_rt_FULLSCREEN=0x80000000u, ons_rt_RLEACCEL=0x4000,
    ons_mix_MAX_VOLUME=128, ons_mix_DEFAULT_FORMAT=0x8010, ons_mix_DEFAULT_CHANNELS=2
};
#define ons_rt_VERSION_ATLEAST(a,b,c) ((a)<1 || ((a)==1 && ((b)<2 || ((b)==2 && (c)<=15))))
#define ons_rt_COMPILEDVERSION 1215
#define ons_rt_TTF_MAJOR_VERSION 2
#define ons_rt_TTF_MINOR_VERSION 0
#define ons_rt_TTF_PATCHLEVEL 10
#define ons_rt_BUTTON(number) (1u << ((number)-1))
#define ons_rt_EVENTMASK(type) (1u << (type))
#define ons_rt_ALLEVENTS 0xffffffffu
#define NR_CALL
typedef struct { ons_rt_Key sym; unsigned mod; uint16_t unicode; } ons_rt_Keysym;
typedef struct { uint8_t type, which, state; ons_rt_Keysym keysym; } ons_rt_KeyboardEvent;
typedef struct { uint8_t type, which, button, state; int x, y; } ons_rt_MouseButtonEvent;
typedef struct { uint8_t type, which, state; int x, y, xrel, yrel; } ons_rt_MouseMotionEvent;
typedef struct { uint8_t type, which, axis; int16_t value; } ons_rt_JoyAxisEvent;
typedef union {
    uint8_t type;
    ons_rt_KeyboardEvent key;
    ons_rt_MouseButtonEvent button;
    ons_rt_MouseMotionEvent motion;
    ons_rt_JoyAxisEvent jaxis;
    struct { uint8_t type, which, button, state; } jbutton;
    struct { uint8_t type, gain, state; } active;
    struct { uint8_t type; int code; void *data1, *data2; } user;
} ons_rt_Event;
typedef struct { int freq; uint16_t format; uint8_t channels, silence; uint16_t samples; uint32_t size; } ons_rt_AudioSpec;
typedef struct ons_rt_timer *ons_rt_TimerID;
typedef struct ons_rt_rwops { uint8_t *memory; size_t length, position; } ons_rt_RWops;
typedef struct { int status; } ons_rt_CD;
typedef ons_font ons_ttf_Font;
typedef struct { int allocated; uint8_t *abuf; uint32_t alen; uint8_t volume; } ons_mix_Chunk;
typedef struct ons_mix_music ons_mix_Music;
#define ons_ttf_STYLE_BOLD ONS_FONT_BOLD
#define ons_ttf_STYLE_NORMAL ONS_FONT_NORMAL
#define ons_ttf_FontAscent ons_font_ascent
#define ons_ttf_FontHeight ons_font_height
#define ons_ttf_GlyphMetrics ons_font_metrics
#define ons_ttf_GetFontStyle ons_font_get_style
#define ons_ttf_SetFontStyle ons_font_style

extern "C" {
int ons_rt_Init(uint32_t flags);
int ons_rt_InitSubSystem(uint32_t flags);
void ons_rt_Quit(void);
const char *ons_rt_GetError(void);
uint32_t ons_rt_GetTicks(void);
void ons_rt_Delay(uint32_t milliseconds);
ons_surface *ons_rt_SetVideoMode(int w, int h, int depth, uint32_t flags);
ons_rect **ons_rt_ListModes(ons_pixel_format *format, uint32_t flags);
void ons_rt_UpdateRect(ons_surface *surface, int x, int y, uint32_t width, uint32_t height);
int ons_rt_BlitSurface(ons_surface *source, ons_rect *from, ons_surface *destination, ons_rect *to);
int ons_rt_FillRect(ons_surface *surface, ons_rect *rect, uint32_t pixel);
ons_surface *ons_rt_ConvertSurface(ons_surface *surface, ons_pixel_format *format, uint32_t flags);
int ons_rt_SaveBMP(ons_surface *surface, const char *path);
uint32_t ons_rt_MapRGB(const ons_pixel_format *format, uint8_t r, uint8_t g, uint8_t b);
uint32_t ons_rt_MapRGBA(const ons_pixel_format *format, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
int ons_rt_PollEvent(ons_rt_Event *event);
int ons_rt_WaitEvent(ons_rt_Event *event);
int ons_rt_PushEvent(ons_rt_Event *event);
int ons_rt_PeepEvents(ons_rt_Event *events, int number, int action, uint32_t mask);
uint8_t ons_rt_GetMouseState(int *x, int *y);
uint8_t ons_rt_GetAppState(void);
void ons_rt_WarpMouse(int x, int y);
int ons_rt_ShowCursor(int visible);
void ons_rt_WM_SetCaption(const char *title, const char *icon);
int ons_rt_WM_ToggleFullScreen(ons_surface *screen);
void *ons_rt_JoystickOpen(int number);
ons_rt_TimerID ons_rt_AddTimer(uint32_t interval, uint32_t (*callback)(uint32_t, void *), void *argument);
int ons_rt_RemoveTimer(ons_rt_TimerID timer);
int ons_rt_CDNumDrives(void);
ons_rt_CD *ons_rt_CDOpen(int number);
int ons_rt_CDStatus(ons_rt_CD *cd);
int ons_rt_CDPlayTracks(ons_rt_CD *cd, int track, int frame, int tracks, int frames);
int ons_rt_CDStop(ons_rt_CD *cd);
void ons_rt_CDClose(ons_rt_CD *cd);
ons_rt_RWops *ons_rt_RWFromMem(void *memory, int size);
int ons_rt_RWclose(ons_rt_RWops *stream);
int ons_img_isPNG(ons_rt_RWops *stream);
ons_surface *ons_img_Load_RW(ons_rt_RWops *stream, int free_stream);
ons_surface *ons_img_LoadJPG_RW(ons_rt_RWops *stream);
const char *ons_img_GetError(void);
int ons_ttf_Init(void);
const char *ons_ttf_GetError(void);
ons_surface *ons_ttf_RenderGlyph_Blended(ons_ttf_Font *font, uint16_t glyph, ons_color color);
ons_surface *ons_ttf_RenderGlyph_Shaded(ons_ttf_Font *font, uint16_t glyph, ons_color color, ons_color background);
int ons_mix_OpenAudio(int rate, uint16_t format, int channels, int samples);
void ons_mix_CloseAudio(void);
int ons_mix_QuerySpec(int *rate, uint16_t *format, int *channels);
int ons_mix_AllocateChannels(int number);
void ons_mix_ChannelFinished(void (*callback)(int));
void ons_mix_HookMusic(void (*callback)(void *, uint8_t *, int), void *argument);
void ons_mix_HookMusicFinished(void (*callback)(void));
ons_mix_Chunk *ons_mix_QuickLoad_RAW(uint8_t *bytes, uint32_t length);
ons_mix_Chunk *ons_mix_LoadWAV_RW(ons_rt_RWops *stream, int free_stream);
void ons_mix_FreeChunk(ons_mix_Chunk *chunk);
int ons_mix_PlayChannel(int channel, ons_mix_Chunk *chunk, int loops);
void ons_mix_Pause(int channel);
int ons_mix_Volume(int channel, int volume);
int ons_mix_VolumeMusic(int volume);
int ons_mix_SetMusicCMD(const char *command);
ons_mix_Music *ons_mix_LoadMUS(const char *path);
ons_mix_Music *ons_mix_LoadMUS_RW(ons_rt_RWops *stream);
int ons_mix_PlayMusic(ons_mix_Music *music, int loops);
int ons_mix_PlayingMusic(void);
int ons_mix_SetMusicPosition(double seconds);
int ons_mix_HaltMusic(void);
void ons_mix_FreeMusic(ons_mix_Music *music);
const char *ons_mix_GetError(void);
void ons_mix_Diagnostics(uint64_t *frames, unsigned *underruns);
void ons_mix_LastSample(int16_t output[2]);
}
#endif
