#include "platform.h"
#include "evdev-hid.h"
#include "libretro.h"
#include "pixels.h"
#include <alsa/asoundlib.h>
#include <dlfcn.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#define API(result,name,args) result (*name) args
static struct {
API(void,retro_init,(void));API(void,retro_deinit,(void));API(unsigned,retro_api_version,(void));
API(void,retro_get_system_info,(struct retro_system_info *));API(void,retro_get_system_av_info,(struct retro_system_av_info *));
API(void,retro_set_environment,(retro_environment_t));API(void,retro_set_video_refresh,(retro_video_refresh_t));
API(void,retro_set_audio_sample,(retro_audio_sample_t));API(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));
API(void,retro_set_input_poll,(retro_input_poll_t));API(void,retro_set_input_state,(retro_input_state_t));
API(void,retro_set_controller_port_device,(unsigned,unsigned));API(bool,retro_load_game,(const struct retro_game_info *));
API(void,retro_unload_game,(void));API(void,retro_run,(void));API(void *,retro_get_memory_data,(unsigned));
API(size_t,retro_get_memory_size,(unsigned));
} api;
#define retro_init api.retro_init
#define retro_deinit api.retro_deinit
#define retro_api_version api.retro_api_version
#define retro_get_system_info api.retro_get_system_info
#define retro_get_system_av_info api.retro_get_system_av_info
#define retro_set_environment api.retro_set_environment
#define retro_set_video_refresh api.retro_set_video_refresh
#define retro_set_audio_sample api.retro_set_audio_sample
#define retro_set_audio_sample_batch api.retro_set_audio_sample_batch
#define retro_set_input_poll api.retro_set_input_poll
#define retro_set_input_state api.retro_set_input_state
#define retro_set_controller_port_device api.retro_set_controller_port_device
#define retro_load_game api.retro_load_game
#define retro_unload_game api.retro_unload_game
#define retro_run api.retro_run
#define retro_get_memory_data api.retro_get_memory_data
#define retro_get_memory_size api.retro_get_memory_size
int retro_present(const void *,unsigned,unsigned,size_t,int,float);
void menu_font_stream_close(void);
static volatile sig_atomic_t quit;
static int headless,failed,paused,format,mode;
static uint16_t touch_pad,key_pad;
static f1_hid keys;
static snd_pcm_t *pcm;
static struct retro_system_av_info av;
static unsigned video_frames,changes,nonblack,last_hash;
static unsigned short audio_head,audio_count;
static int16_t ring[8192*2];
static uint64_t audio_received,audio_written,audio_dropped;
static struct {char key[128],value[128];} options[128];
static unsigned option_count;
static const char *system_dir,*save_dir;
static lv_obj_t *pause_label;
static void stop(int sig){(void)sig;quit=1;}
static int64_t now_us(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (int64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
static void log_core(enum retro_log_level level,const char *text,...)
{(void)level;va_list args;va_start(args,text);vfprintf(stderr,text,args);va_end(args);}
static bool environment(unsigned command,void *data)
{
    switch(command){
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:{int f=*(enum retro_pixel_format *)data;if(f<0||f>2)return false;format=f;return true;}
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:*(bool *)data=true;return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:*(const char **)data=system_dir;return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:*(const char **)data=save_dir;return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:((struct retro_log_callback *)data)->log=log_core;return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:*(unsigned *)data=0;return true;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:return true;
    case RETRO_ENVIRONMENT_GET_LANGUAGE:*(unsigned *)data=RETRO_LANGUAGE_ENGLISH;return true;
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:*(int *)data=3;return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:*(bool *)data=false;return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES:{const struct retro_variable *v=data;option_count=0;
        for(;v&&v->key&&option_count<128;v++){const char *value=strchr(v->value,';');if(!value)continue;value++;while(*value==' ')value++;
            snprintf(options[option_count].key,128,"%s",v->key);size_t n=strcspn(value,"|");if(n>127)n=127;
            memcpy(options[option_count].value,value,n);options[option_count].value[n]=0;option_count++;}return true;}
    case RETRO_ENVIRONMENT_GET_VARIABLE:{struct retro_variable *v=data;v->value=NULL;
        if(!strcmp(v->key,"pcsx_rearmed_drc")){v->value="enabled";return true;}
        if(mode==2&&!strcmp(v->key,"picodrive_input1")){v->value="6 button pad";return true;}
        for(unsigned i=0;i<option_count;i++)if(!strcmp(v->key,options[i].key)){v->value=options[i].value;return true;}
        return false;}
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:return true;
    case RETRO_ENVIRONMENT_SET_GEOMETRY:av.geometry=*(struct retro_game_geometry *)data;return true;
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:av=*(struct retro_system_av_info *)data;return true;
    case RETRO_ENVIRONMENT_SHUTDOWN:quit=1;return true;
    default:return false;
    }
}
static void video(const void *p,unsigned w,unsigned h,size_t pitch)
{
    if(!p)return;
    if(p==RETRO_HW_FRAME_BUFFER_VALID||!w||w>1024||!h||h>512||pitch<w*(format==1?4:2)||pitch>65536){failed=quit=1;return;}
    if(headless){uint32_t hash=2166136261u;nonblack=0;
        for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){uint32_t c=retro_pixel((const uint8_t*)p+y*pitch+x*(format==1?4:2),format);nonblack+=c!=0;hash=(hash^c)*16777619u;}
        if(video_frames&&hash!=last_hash)changes++;
        last_hash=hash;
    }else if(retro_present(p,w,h,pitch,format,av.geometry.aspect_ratio)){failed=quit=1;}
    video_frames++;
}
static void audio_flush(void)
{
    for(int attempt=0;pcm&&audio_count&&attempt<4;attempt++){
        unsigned n=8192-audio_head;if(n>audio_count)n=audio_count;
        snd_pcm_sframes_t wrote=snd_pcm_writei(pcm,ring+audio_head*2,n);
        if(wrote==-EAGAIN||!wrote)break;
        if(wrote<0){if(snd_pcm_recover(pcm,wrote,1)<0){snd_pcm_close(pcm);pcm=NULL;}break;}
        audio_head=(audio_head+wrote)%8192;audio_count-=wrote;audio_written+=wrote;
    }
}
static size_t audio_batch(const int16_t *data,size_t frames)
{
    audio_received+=frames;
    if(headless||!pcm)return frames;
    size_t accepted=frames;
    if(frames>8192){audio_dropped+=frames-8192;data+=(frames-8192)*2;frames=8192;}
    if(audio_count+frames>8192){unsigned drop=audio_count+frames-8192;audio_head=(audio_head+drop)%8192;audio_count-=drop;audio_dropped+=drop;}
    for(unsigned i=0;i<frames;i++){unsigned at=(audio_head+audio_count+i)%8192;ring[at*2]=data[i*2];ring[at*2+1]=data[i*2+1];}
    audio_count+=frames;audio_flush();return accepted;
}
static void audio_sample(int16_t l,int16_t r){int16_t pair[]={l,r};audio_batch(pair,1);}
static uint16_t key_mask(unsigned key)
{
    unsigned id;
    switch(key){case F1_UP:id=RETRO_DEVICE_ID_JOYPAD_UP;break;case F1_DOWN:id=RETRO_DEVICE_ID_JOYPAD_DOWN;break;
    case F1_LEFT:id=RETRO_DEVICE_ID_JOYPAD_LEFT;break;case F1_RIGHT:id=RETRO_DEVICE_ID_JOYPAD_RIGHT;break;
    case 'z':case 'Z':id=RETRO_DEVICE_ID_JOYPAD_B;break;case 'x':case 'X':id=RETRO_DEVICE_ID_JOYPAD_A;break;
    case 'a':case 'A':id=RETRO_DEVICE_ID_JOYPAD_Y;break;case 's':case 'S':id=RETRO_DEVICE_ID_JOYPAD_X;break;
    case 'q':case 'Q':id=RETRO_DEVICE_ID_JOYPAD_L;break;case 'w':case 'W':id=RETRO_DEVICE_ID_JOYPAD_R;break;
    case 'e':case 'E':id=RETRO_DEVICE_ID_JOYPAD_L2;break;case 'r':case 'R':id=RETRO_DEVICE_ID_JOYPAD_R2;break;
    case '\n':id=RETRO_DEVICE_ID_JOYPAD_START;break;case F1_NEXT:id=RETRO_DEVICE_ID_JOYPAD_SELECT;break;default:return 0;}
    return 1u<<id;
}
static void input_poll(void)
{
    if(headless)return;
    f1_hid_poll(&keys,platform_ms(),480,800);
    while(keys.kn){f1_hid_key k=keys.keys[keys.kh];keys.kh=(keys.kh+1)%F1_HID_EVENTS;keys.kn--;
        if(k.gamepad)continue;
        if(k.key==27&&k.down)quit=1;
        unsigned mask=key_mask(k.key);if(k.down)key_pad|=mask;else key_pad&=~mask;}
    keys.pn=0;
}
static int16_t input_state(unsigned port,unsigned device,unsigned index,unsigned id)
{
    (void)index;if(port||device!=RETRO_DEVICE_JOYPAD)return 0;
    unsigned pad=touch_pad|key_pad|f1_hid_pad(&keys);if(id==RETRO_DEVICE_ID_JOYPAD_MASK)return pad;
    return id<16&&(pad&(1u<<id))?1:0;
}
static void pressed(lv_event_t *e)
{unsigned mask=(uintptr_t)lv_event_get_user_data(e);int code=lv_event_get_code(e);
 if(code==LV_EVENT_PRESSED)touch_pad|=mask;
 if(code==LV_EVENT_RELEASED||code==LV_EVENT_PRESS_LOST)touch_pad&=~mask;}
static void ui_command(lv_event_t *e)
{if(lv_event_get_user_data(e))quit=1;else{paused=!paused;touch_pad=key_pad=0;audio_count=0;
 lv_label_set_text(pause_label,paused?LV_SYMBOL_PLAY:LV_SYMBOL_PAUSE);
 if(pcm){snd_pcm_drop(pcm);snd_pcm_prepare(pcm);}}}
static lv_obj_t *button(int x,int y,int width,const char *text,int id)
{
    lv_obj_t *o=lv_btn_create(lv_scr_act());lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,width,60);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_bg_color(o,lv_color_hex(0x293a40),0);
    lv_obj_set_style_bg_color(o,lv_color_hex(0x167a6f),LV_STATE_PRESSED);lv_obj_set_style_radius(o,4,0);
    lv_obj_t *l=lv_label_create(o);lv_label_set_text(l,text);lv_obj_set_style_text_font(l,&lv_font_montserrat_24,0);lv_obj_set_style_text_color(l,lv_color_white(),0);lv_obj_center(l);
    if(id>=0)lv_obj_add_event_cb(o,pressed,LV_EVENT_ALL,(void *)(uintptr_t)(1u<<id));
    return o;
}
static void ui(void)
{
    lv_obj_clear_flag(lv_scr_act(),LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(lv_scr_act(),lv_color_black(),0);
    button(8,452,112,"SELECT",RETRO_DEVICE_ID_JOYPAD_SELECT);button(128,452,112,"START",RETRO_DEVICE_ID_JOYPAD_START);
    lv_obj_t *pause=button(248,452,104,LV_SYMBOL_PAUSE,-1);pause_label=lv_obj_get_child(pause,0);
    lv_obj_add_event_cb(pause,ui_command,LV_EVENT_CLICKED,NULL);
    lv_obj_add_event_cb(button(360,452,112,LV_SYMBOL_CLOSE,-1),ui_command,LV_EVENT_CLICKED,(void*)1);
    button(80,524,64,LV_SYMBOL_UP,RETRO_DEVICE_ID_JOYPAD_UP);button(8,590,64,LV_SYMBOL_LEFT,RETRO_DEVICE_ID_JOYPAD_LEFT);
    button(152,590,64,LV_SYMBOL_RIGHT,RETRO_DEVICE_ID_JOYPAD_RIGHT);button(80,656,64,LV_SYMBOL_DOWN,RETRO_DEVICE_ID_JOYPAD_DOWN);
    if(mode!=0){button(336,524,64,mode==1?"TRI":"Y",RETRO_DEVICE_ID_JOYPAD_X);
        button(264,590,64,mode==1?"SQR":"A",RETRO_DEVICE_ID_JOYPAD_Y);}
    button(408,590,64,mode==1?"O":mode==2?"C":"A",RETRO_DEVICE_ID_JOYPAD_A);
    button(336,656,64,mode==1?"X":"B",RETRO_DEVICE_ID_JOYPAD_B);
    button(8,730,104,mode==2?"X":"L",RETRO_DEVICE_ID_JOYPAD_L);
    if(mode==1){button(120,730,104,"L2",RETRO_DEVICE_ID_JOYPAD_L2);button(256,730,104,"R2",RETRO_DEVICE_ID_JOYPAD_R2);}
    button(368,730,104,mode==2?"Z":"R",RETRO_DEVICE_ID_JOYPAD_R);
    for(lv_indev_t *i=lv_indev_get_next(NULL);i;i=lv_indev_get_next(i))if(lv_indev_get_type(i)==LV_INDEV_TYPE_KEYPAD)lv_indev_enable(i,false);
    lv_refr_now(NULL);
}
static int memory_file(const char *rom,int save)
{
    size_t bytes=retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);void *memory=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    if(!memory||!bytes||bytes>1024*1024)return 0;
    char path[PATH_MAX],temp[PATH_MAX];if(snprintf(path,sizeof(path),"%s.native%d.sav",rom,mode)>=(int)sizeof(path)||snprintf(temp,sizeof(temp),"%s.tmp-%ld",path,(long)getpid())>=(int)sizeof(temp))return -1;
    if(!save){FILE *f=fopen(path,"rb");if(f){fread(memory,1,bytes,f);fclose(f);}return 0;}
    int fd=open(temp,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0)return -1;size_t off=0;
    while(off<bytes){ssize_t n=write(fd,(uint8_t*)memory+off,bytes-off);if(n<0&&errno==EINTR)continue;if(n<=0)break;off+=n;}
    int okay=off==bytes&&!fsync(fd);if(close(fd))okay=0;if(okay)okay=rename(temp,path)==0;if(!okay)unlink(temp);return okay?0:-1;
}
int main(int argc,char **argv)
{
    int probe=argc==3&&!strcmp(argv[1],"--probe");headless=argc==6&&!strcmp(argv[1],"--headless");
    unsigned limit=headless?strtoul(argv[2],NULL,10):0;int at=headless?3:1;
    if(!probe&&((!headless&&argc!=4)||(headless&&(!limit||limit>36000))))return 1;
    const char *type=probe?"gba":argv[at],*core=probe?argv[2]:argv[at+1],*rom=probe?NULL:argv[at+2];
    mode=!strcmp(type,"gba")?0:!strcmp(type,"ps1")?1:!strcmp(type,"md")?2:-1;if(mode<0)return 1;
    system_dir=getenv("RETRO_SYSTEM_DIR");if(!system_dir)system_dir="./bios";
    save_dir=getenv("RETRO_SAVE_DIR");if(!save_dir)save_dir="./saves";
    void *library=dlopen(core,RTLD_NOW|RTLD_LOCAL);if(!library){fprintf(stderr,"core load: %s\n",dlerror());return 1;}
#define LOAD(name) do{*(void **)(&name)=dlsym(library,#name);if(!name){fprintf(stderr,"missing %s\n",#name);dlclose(library);return 1;}}while(0)
    LOAD(retro_init);LOAD(retro_deinit);LOAD(retro_api_version);LOAD(retro_get_system_info);LOAD(retro_get_system_av_info);
    LOAD(retro_set_environment);LOAD(retro_set_video_refresh);LOAD(retro_set_audio_sample);LOAD(retro_set_audio_sample_batch);
    LOAD(retro_set_input_poll);LOAD(retro_set_input_state);LOAD(retro_set_controller_port_device);LOAD(retro_load_game);
    LOAD(retro_unload_game);LOAD(retro_run);LOAD(retro_get_memory_data);LOAD(retro_get_memory_size);
    if(retro_api_version()!=RETRO_API_VERSION){dlclose(library);return 1;}
    retro_set_environment(environment);retro_set_video_refresh(video);retro_set_audio_sample(audio_sample);retro_set_audio_sample_batch(audio_batch);
    retro_set_input_poll(input_poll);retro_set_input_state(input_state);retro_init();
    struct retro_system_info info={0};retro_get_system_info(&info);fprintf(stderr,"core=%s version=%s fullpath=%d\n",info.library_name,info.library_version,info.need_fullpath);
    int loaded=0,opened=0,result=1;
    if(probe){result=0;goto cleanup;}
    struct stat st;if(!info.need_fullpath||stat(rom,&st)||!S_ISREG(st.st_mode)||st.st_size<=0)goto cleanup;
    if(!headless)mkdir(save_dir,0755);
    struct retro_game_info game={.path=rom};loaded=retro_load_game(&game);if(!loaded)goto cleanup;
    retro_get_system_av_info(&av);if(!isfinite(av.timing.fps)||av.timing.fps<1||av.timing.fps>240)goto cleanup;
    retro_set_controller_port_device(0,RETRO_DEVICE_JOYPAD);f1_hid_init(&keys);signal(SIGTERM,stop);signal(SIGINT,stop);
    if(!headless){
        mkdir(save_dir,0755);memory_file(rom,0);
        if(platform_open()){platform_close();goto cleanup;}opened=1;ui();
        if(av.timing.sample_rate>=8000&&av.timing.sample_rate<=192000){int rc=snd_pcm_open(&pcm,"default",SND_PCM_STREAM_PLAYBACK,SND_PCM_NONBLOCK);
            if(rc>=0)rc=snd_pcm_set_params(pcm,SND_PCM_FORMAT_S16_LE,SND_PCM_ACCESS_RW_INTERLEAVED,2,av.timing.sample_rate,1,100000);
            if(rc<0){fprintf(stderr,"audio: %s\n",snd_strerror(rc));if(pcm)snd_pcm_close(pcm);pcm=NULL;}}
    }
    unsigned runs=0;int64_t deadline=now_us(),started=deadline;uint64_t tick=headless?0:platform_ms();
    while(!quit&&(!headless||runs<limit)){
        if(!headless){uint64_t t=platform_ms();lv_tick_inc(t-tick);tick=t;lv_timer_handler();input_poll();if(paused){deadline=now_us();usleep(10000);continue;}}
        retro_run();runs++;audio_flush();
        if(!headless){deadline+=(int64_t)(1000000/av.timing.fps);int64_t delay=deadline-now_us();if(delay>0&&delay<100000)usleep(delay);else if(delay< -100000)deadline=now_us();}
    }
    fprintf(stderr,"native runs=%u video=%u changes=%u nonblack=%u hash=%08x audio_received=%llu audio_written=%llu audio_dropped=%llu elapsed_us=%lld\n",runs,video_frames,changes,nonblack,last_hash,(unsigned long long)audio_received,(unsigned long long)audio_written,(unsigned long long)audio_dropped,(long long)(now_us()-started));
    if(!headless&&memory_file(rom,1))fprintf(stderr,"save RAM write failed\n");
    f1_hid_close(&keys);result=failed||(headless&&runs!=limit)?1:0;
cleanup:
    if(pcm){snd_pcm_drop(pcm);snd_pcm_close(pcm);}if(opened){platform_close();menu_font_stream_close();}
    if(loaded)retro_unload_game();
    retro_deinit();dlclose(library);return result;
}
