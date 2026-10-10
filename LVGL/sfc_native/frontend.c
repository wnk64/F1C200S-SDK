#include "platform.h"
#include "evdev-hid.h"
#include "snes9x.h"
#include "soundux.h"
#include "memmap.h"
#include "apu.h"
#include "display.h"
#include "gfx.h"
#include "cpuexec.h"
#include "ppu.h"
#include <alsa/asoundlib.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

extern const lv_font_t menu_font_stream;
void menu_font_stream_close(void);
int sfc_present(const uint16_t *, unsigned, unsigned, unsigned);
static volatile sig_atomic_t quit;
static uint16_t *pixels;
static uint8_t *sub, *z, *subz;
static uint32_t touch_pad, keyboard_pad;
static int paused, frameskip = 1;
static f1_hid game_input;
static snd_pcm_t *pcm;
static int16_t audio_ring[4096*2];
static unsigned audio_head, audio_count;
static uint64_t audio_fraction, audio_dropped, audio_written;
static lv_obj_t *pause_label;
static void stop(int sig) { (void)sig; quit = 1; }
static int64_t now_us(void)
{ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (int64_t)t.tv_sec*1000000+t.tv_nsec/1000; }
static uint32_t key_mask(unsigned key)
{
    switch (key) {
    case F1_UP:return SNES_UP_MASK; case F1_DOWN:return SNES_DOWN_MASK;
    case F1_LEFT:return SNES_LEFT_MASK; case F1_RIGHT:return SNES_RIGHT_MASK;
    case 'z':case 'Z':return SNES_B_MASK; case 'x':case 'X':return SNES_A_MASK;
    case 'a':case 'A':return SNES_Y_MASK; case 's':case 'S':return SNES_X_MASK;
    case 'q':case 'Q':return SNES_TL_MASK; case 'w':case 'W':return SNES_TR_MASK;
    case '\n':return SNES_START_MASK; case F1_NEXT:return SNES_SELECT_MASK;
    default:return 0;
    }
}
uint32_t S9xReadJoypad(int32_t port)
{
    if(port)return 0;
    static const uint32_t masks[]={SNES_B_MASK,SNES_Y_MASK,SNES_SELECT_MASK,SNES_START_MASK,
        SNES_UP_MASK,SNES_DOWN_MASK,SNES_LEFT_MASK,SNES_RIGHT_MASK,SNES_A_MASK,SNES_X_MASK,SNES_TL_MASK,SNES_TR_MASK};
    uint32_t pad=touch_pad|keyboard_pad;unsigned buttons=f1_hid_pad(&game_input);
    for(unsigned i=0;i<sizeof(masks)/sizeof(*masks);i++)if(buttons&(1u<<i))pad|=masks[i];
    return pad;
}
bool S9xReadMousePosition(int32_t n,int32_t *x,int32_t *y,uint32_t *b)
{ (void)n;(void)x;(void)y;(void)b;return false; }
bool S9xReadSuperScopePosition(int32_t *x,int32_t *y,uint32_t *b)
{ (void)x;(void)y;(void)b;return false; }
bool JustifierOffscreen(void) { return true; }
void JustifierButtons(uint32_t *b) { (void)b; }
bool S9xInitDisplay(void)
{
    /* The core can switch to 512-wide/interlaced output during a scanline. */
    pixels=calloc(512*478,6);
    if(!pixels)return false;
    sub=(uint8_t *)pixels+512*478*2;z=sub+512*478*2;subz=z+512*478;
    GFX.Pitch=1024; GFX.ZPitch=512; GFX.Screen=(uint8_t *)pixels; GFX.SubScreen=sub; GFX.ZBuffer=z; GFX.SubZBuffer=subz;
    return true;
}
void S9xDeinitDisplay(void) { free(pixels);pixels=NULL; }
void S9xToggleSoundChannel(int32_t n) { (void)n; }
static void control(lv_event_t *e)
{
    uint32_t mask=(uintptr_t)lv_event_get_user_data(e);
    if(lv_event_get_code(e)==LV_EVENT_PRESSED) touch_pad|=mask;
    if(lv_event_get_code(e)==LV_EVENT_RELEASED||lv_event_get_code(e)==LV_EVENT_PRESS_LOST) touch_pad&=~mask;
}
static void command(lv_event_t *e)
{
    if ((uintptr_t)lv_event_get_user_data(e)) quit=1;
    else {
        paused=!paused; touch_pad=keyboard_pad=0; audio_count=0;
        if(pcm) { snd_pcm_drop(pcm); snd_pcm_prepare(pcm); }
        lv_label_set_text(pause_label,paused?LV_SYMBOL_PLAY:LV_SYMBOL_PAUSE);
    }
}
static lv_obj_t *button(int x,int y,int w,const char *text,uint32_t mask)
{
    lv_obj_t *b=lv_btn_create(lv_scr_act());lv_obj_remove_style_all(b);
    lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,64);
    lv_obj_set_style_bg_opa(b,LV_OPA_COVER,0);lv_obj_set_style_radius(b,4,0);
    lv_obj_set_style_bg_color(b,lv_color_hex(0x293a40),0);
    lv_obj_set_style_bg_color(b,lv_color_hex(0x167a6f),LV_STATE_PRESSED);
    lv_obj_t *l=lv_label_create(b);lv_label_set_text(l,text);lv_obj_set_style_text_font(l,&lv_font_montserrat_24,0);
    lv_obj_set_style_text_color(l,lv_color_white(),0);lv_obj_center(l);
    if(mask)lv_obj_add_event_cb(b,control,LV_EVENT_ALL,(void *)(uintptr_t)mask);
    return b;
}
static void ui(void)
{
    lv_obj_clear_flag(lv_scr_act(),LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(lv_scr_act(),lv_color_black(),0);
    button(8,456,112,"SELECT",SNES_SELECT_MASK); button(128,456,112,"START",SNES_START_MASK);
    lv_obj_t *p=button(248,456,104,LV_SYMBOL_PAUSE,0);pause_label=lv_obj_get_child(p,0);
    lv_obj_add_event_cb(p,command,LV_EVENT_CLICKED,NULL);
    lv_obj_t *exit=button(360,456,112,LV_SYMBOL_CLOSE,0);
    lv_obj_add_event_cb(exit,command,LV_EVENT_CLICKED,(void *)1);
    button(80,536,64,LV_SYMBOL_UP,SNES_UP_MASK);button(8,608,64,LV_SYMBOL_LEFT,SNES_LEFT_MASK);
    button(152,608,64,LV_SYMBOL_RIGHT,SNES_RIGHT_MASK);button(80,664,64,LV_SYMBOL_DOWN,SNES_DOWN_MASK);
    button(336,536,64,"X",SNES_X_MASK);button(264,608,64,"Y",SNES_Y_MASK);
    button(408,608,64,"A",SNES_A_MASK);button(336,664,64,"B",SNES_B_MASK);
    button(8,736,208,"L",SNES_TL_MASK);button(264,736,208,"R",SNES_TR_MASK);
    for(lv_indev_t *i=lv_indev_get_next(NULL);i;i=lv_indev_get_next(i))
        if(lv_indev_get_type(i)==LV_INDEV_TYPE_KEYPAD)lv_indev_enable(i,false);
    lv_refr_now(NULL);
}
static void audio_open(void)
{
    int rc=snd_pcm_open(&pcm,"default",SND_PCM_STREAM_PLAYBACK,SND_PCM_NONBLOCK);
    if(rc>=0)rc=snd_pcm_set_params(pcm,SND_PCM_FORMAT_S16_LE,SND_PCM_ACCESS_RW_INTERLEAVED,2,32000,1,100000);
    if(rc<0){fprintf(stderr,"SFC audio unavailable: %s\n",snd_strerror(rc));if(pcm)snd_pcm_close(pcm);pcm=NULL;}
}
static void audio_step(void)
{
    int16_t samples[1600];
    audio_fraction+=(uint64_t)32000*Settings.FrameTime;
    unsigned frames=audio_fraction/1000000;audio_fraction%=1000000;
    if(frames>800)frames=800;
    S9xMixSamples(samples,frames*2);
    if(!pcm)return;
    if(audio_count+frames>4096){unsigned drop=audio_count+frames-4096;audio_head=(audio_head+drop)%4096;audio_count-=drop;audio_dropped+=drop;}
    for(unsigned i=0;i<frames;i++){unsigned at=(audio_head+audio_count+i)%4096;audio_ring[at*2]=samples[i*2];audio_ring[at*2+1]=samples[i*2+1];}
    audio_count+=frames;
    for(unsigned budget=0;budget<4&&audio_count;budget++) {
        unsigned n=4096-audio_head;if(n>audio_count)n=audio_count;
        snd_pcm_sframes_t wrote=snd_pcm_writei(pcm,audio_ring+audio_head*2,n);
        if(wrote==-EAGAIN)break;
        if(wrote<0){if(snd_pcm_recover(pcm,wrote,1)<0){snd_pcm_close(pcm);pcm=NULL;}break;}
        if(!wrote)break;
        audio_head=(audio_head+wrote)%4096;audio_count-=wrote;audio_written+=wrote;
    }
}
static void sram(const char *rom,int save)
{
    if(!Memory.SRAMSize||Memory.SRAMSize>16)return;
    char path[PATH_MAX],temporary[PATH_MAX];
    if(snprintf(path,sizeof(path),"%s.srm",rom)>=(int)sizeof(path)||snprintf(temporary,sizeof(temporary),"%s.tmp-%ld",path,(long)getpid())>=(int)sizeof(temporary))return;
    size_t bytes=(size_t)1024<<Memory.SRAMSize;if(bytes>SRAM_SIZE)bytes=SRAM_SIZE;
    if(!save){FILE *f=fopen(path,"rb");if(f){(void)fread(Memory.SRAM,1,bytes,f);fclose(f);}return;}
    int fd=open(temporary,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0)return;
    size_t off=0;while(off<bytes){ssize_t n=write(fd,Memory.SRAM+off,bytes-off);if(n<0&&errno==EINTR)continue;if(n<=0)break;off+=n;}
    int okay=off==bytes&&!fsync(fd);if(close(fd))okay=0;
    if(okay)okay=rename(temporary,path)==0;
    if(!okay){perror("SFC SRAM save");unlink(temporary);}
}
int main(int argc,char **argv)
{
    int headless=argc==4&&!strcmp(argv[1],"--headless");
    const char *rom=headless?argv[3]:argc==2?argv[1]:NULL;
    unsigned limit=headless?strtoul(argv[2],NULL,10):0;
    struct stat st;
    if(!rom||(headless&&(!limit||limit>36000))||stat(rom,&st)||!S_ISREG(st.st_mode)||st.st_size<32768||st.st_size>8*1024*1024+512){fprintf(stderr,"usage: sfc-native [--headless frames] ROM.sfc (32KiB..8MiB)\n");return 1;}
    signal(SIGINT,stop);signal(SIGTERM,stop);
    memset(&Settings,0,sizeof(Settings));Settings.CyclesPercentage=100;Settings.H_Max=SNES_CYCLES_PER_SCANLINE;
    Settings.FrameTimePAL=20000;Settings.FrameTimeNTSC=16667;Settings.FrameTime=16667;
    Settings.ControllerOption=SNES_JOYPAD;Settings.HBlankStart=(256*Settings.H_Max)/SNES_HCOUNTER_MAX;
    Settings.SoundPlaybackRate=Settings.SoundInputRate=32000;Settings.InterpolatedSound=true;
    Settings.APUEnabled=Settings.NextAPUEnabled=true;Settings.Shutdown=true;
    int memory_ok=0,apu_ok=0,gfx_ok=0,result=1,opened=0;
    if(!S9xInitDisplay()||!(memory_ok=S9xInitMemory())||!(apu_ok=S9xInitAPU())||!S9xInitSound(0,0)||!(gfx_ok=S9xInitGFX())||!LoadROM(rom))goto cleanup;
    S9xSetPlaybackRate(32000);if(!headless)sram(rom,0);
    if(!headless){if(platform_open()){platform_close();goto cleanup;}opened=1;ui();audio_open();}
    f1_hid_init(&game_input);
    const char *skip=getenv("SFC_FRAMESKIP");if(skip){int n=atoi(skip);if(n>=0&&n<=5)frameskip=n;}
    unsigned frames=0,draws=0,changes=0,nonblack=0;uint32_t hash=2166136261u,last_hash=0;int64_t started=now_us(),deadline=started;
    uint64_t tick=headless?0:platform_ms();
    while(!quit&&(!headless||frames<limit)) {
        if(!headless){
            uint64_t t=platform_ms();lv_tick_inc(t-tick);tick=t;lv_timer_handler();
            f1_hid_poll(&game_input,t,480,800);
            while(game_input.kn){f1_hid_key key=game_input.keys[game_input.kh];game_input.kh=(game_input.kh+1)%F1_HID_EVENTS;game_input.kn--;
                if(key.gamepad)continue;
                if(key.key==27&&key.down)quit=1;
                uint32_t mask=key_mask(key.key);if(key.down)keyboard_pad|=mask;else keyboard_pad&=~mask;}
            game_input.pn=0;
            if(paused){deadline=now_us();usleep(10000);continue;}
        }
        IPPU.RenderThisFrame=headless||frames%(frameskip+1)==0;
        GFX.Screen=(uint8_t *)pixels;S9xMainLoop();frames++;
        if(IPPU.RenderThisFrame){
            int h=IPPU.RenderedScreenHeight,w=IPPU.RenderedScreenWidth;
            if(h<=0||h>478||w<=0||w>512){fprintf(stderr,"invalid core dimensions %dx%d\n",w,h);goto cleanup;}
            if(!headless&&sfc_present(pixels,w,h,512)){fprintf(stderr,"SFC framebuffer format unsupported\n");goto cleanup;}
            if(headless){uint32_t frame_hash=2166136261u;nonblack=0;
                for(int y=0;y<h;y++)for(int x=0;x<w;x++){uint16_t c=pixels[y*512+x];frame_hash^=c;frame_hash*=16777619u;nonblack+=c!=0;}
                if(draws&&frame_hash!=last_hash)changes++;
                last_hash=frame_hash;hash^=frame_hash;hash*=16777619u;}
            draws++;
        }
        audio_step();
        if(!headless){deadline+=Settings.FrameTime;int64_t wait=deadline-now_us();if(wait>0&&wait<50000)usleep(wait);else if(wait< -100000)deadline=now_us();}
    }
    fprintf(stderr,"SFC frames=%u draws=%u changes=%u nonblack=%u elapsed_us=%lld hash=%08x audio_written=%llu audio_dropped=%llu\n",frames,draws,changes,nonblack,(long long)(now_us()-started),hash,(unsigned long long)audio_written,(unsigned long long)audio_dropped);
    if(!headless)sram(rom,1);
    result=0;
    f1_hid_close(&game_input);
cleanup:
    if(pcm){snd_pcm_drop(pcm);snd_pcm_close(pcm);}
    if(opened){platform_close();menu_font_stream_close();}
    if(gfx_ok)S9xDeinitGFX();
    if(apu_ok)S9xDeinitAPU();
    if(memory_ok)S9xDeinitMemory();
    S9xDeinitDisplay();return result;
}
