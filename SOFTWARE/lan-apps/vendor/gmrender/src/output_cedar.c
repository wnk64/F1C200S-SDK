#define _GNU_SOURCE
#include "output_module.h"
#include "upnp_connmgr.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static char *uri;
static GPid player,waiting;
static int input=-1,output=-1,paused,muted,closing;
static double duration,position;
static float volume=1;
static output_transition_cb_t feedback;
static char replies[4096];static size_t reply_used;
static int touch_fd=-1,touch_x,touch_y,touch_down,touch_slot,tap_count;
static gint64 tap_started;
static const char *backend="/root/roms/candidates/mplayer_geometry_20261005/mplayer-native-geometry";
void cedar_shutdown(void);
static void command(const char *s){if(input>=0){ssize_t n=write(input,s,strlen(s));if(n<0&&errno!=EAGAIN)fprintf(stderr,"DLNA command write failed\n");}}
static void reap(GPid *pid){
    if(!*pid)return;
    kill(*pid,SIGTERM);for(int i=0;i<40;i++){if(waitpid(*pid,NULL,WNOHANG)==*pid){*pid=0;return;}usleep(25000);}
    kill(*pid,SIGKILL);waitpid(*pid,NULL,0);*pid=0;
}
static void wait_ui(void){
    if(closing||waiting)return;
    char *args[]={"./lan-ui","--dlna",NULL};GError *error=NULL;
    if(!g_spawn_async(NULL,args,NULL,G_SPAWN_DO_NOT_REAP_CHILD,NULL,NULL,&waiting,&error)){
        fprintf(stderr,"DLNA waiting UI: %s\n",error->message);g_error_free(error);waiting=0;
    }
}
static void release_player(void){reap(&player);if(input>=0)close(input);if(output>=0)close(output);input=output=-1;paused=0;reply_used=0;}
static gboolean monitor(gpointer unused){
    (void)unused;output_transition_cb_t done=NULL;pthread_mutex_lock(&lock);
    if(waiting&&waitpid(waiting,NULL,WNOHANG)==waiting){waiting=0;closing=1;kill(getpid(),SIGTERM);}
    if(player){
        struct input_event event;
        while(touch_fd>=0&&read(touch_fd,&event,sizeof(event))==sizeof(event)){
            if(event.type==EV_ABS&&event.code==ABS_MT_SLOT)touch_slot=event.value;
            if(event.type==EV_ABS&&!touch_slot){
                if(event.code==ABS_MT_POSITION_X||event.code==ABS_X)touch_x=event.value;
                if(event.code==ABS_MT_POSITION_Y||event.code==ABS_Y)touch_y=event.value;
            }
            int edge=-1;
            if(event.type==EV_KEY&&event.code==BTN_TOUCH)edge=!!event.value;
            if(event.type==EV_ABS&&!touch_slot&&event.code==ABS_MT_TRACKING_ID)edge=event.value>=0;
            if(edge==0&&touch_down){
                gint64 now=g_get_monotonic_time();
                if(touch_x>=120&&touch_x<=360&&touch_y>=200&&touch_y<=600){
                    if(now-tap_started>2000000){tap_count=0;tap_started=now;}
                    if(++tap_count>=3){closing=1;kill(getpid(),SIGTERM);}
                }else tap_count=0;
            }
            if(edge>=0)touch_down=edge;
        }
        command("pausing_keep_force get_time_pos\npausing_keep_force get_time_length\n");
        char data[512];ssize_t count;
        while((count=read(output,data,sizeof(data)))>0)for(ssize_t i=0;i<count;i++){
            if(data[i]=='\n'){replies[reply_used]=0;if(!strncmp(replies,"ANS_TIME_POSITION=",18))position=atof(replies+18);if(!strncmp(replies,"ANS_LENGTH=",11))duration=atof(replies+11);reply_used=0;}
            else if(reply_used<sizeof(replies)-1)replies[reply_used++]=data[i];else reply_used=0;
        }
        if(waitpid(player,NULL,WNOHANG)==player){player=0;release_player();done=feedback;feedback=NULL;wait_ui();}
    }
    pthread_mutex_unlock(&lock);if(done)done(PLAY_STOPPED);return TRUE;
}
static int init(void){
    signal(SIGPIPE,SIG_IGN);setenv("CEDAR_ROTATE","90",1);setenv("CEDAR_CMA_POOL_MB","12",1);setenv("TERM","dumb",1);
    register_mime_type("video/mp4");register_mime_type("audio/mpeg");register_mime_type("audio/mp4");register_mime_type("audio/aac");
    touch_fd=open("/dev/input/event0",O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    atexit(cedar_shutdown);wait_ui();g_timeout_add(500,monitor,NULL);return 0;
}
static void set_uri(const char *value,output_update_meta_cb_t unused){
    (void)unused;pthread_mutex_lock(&lock);release_player();free(uri);uri=NULL;
    if(value&&strlen(value)<8192&&(!strncmp(value,"http://",7)||!strncmp(value,"https://",8))&&!strchr(value,'\n')&&!strchr(value,'\r'))uri=strdup(value);
    position=duration=0;pthread_mutex_unlock(&lock);
}
static int play(output_transition_cb_t cb){
    pthread_mutex_lock(&lock);if(player){if(paused){command("pause\n");paused=0;}pthread_mutex_unlock(&lock);return 0;}
    if(!uri){pthread_mutex_unlock(&lock);return -1;}reap(&waiting);
    char volume_arg[16];snprintf(volume_arg,sizeof(volume_arg),"%.0f",muted?0:volume*100);
    char *args[]={(char *)backend,"-slave","-noconsolecontrols","-quiet","-noconfig","all","-softvol","-softvol-max","100","-volume",volume_arg,"-afm","faad","-vc","cedarh264","-vo","cedar_drm:native","-ao","alsa",uri,NULL};
    GError *error=NULL;int ok=g_spawn_async_with_pipes(NULL,args,NULL,G_SPAWN_DO_NOT_REAP_CHILD,NULL,NULL,&player,&input,&output,NULL,&error);
    if(!ok){fprintf(stderr,"DLNA player: %s\n",error->message);g_error_free(error);wait_ui();pthread_mutex_unlock(&lock);return -1;}
    fcntl(input,F_SETFL,O_NONBLOCK);fcntl(output,F_SETFL,O_NONBLOCK);feedback=cb;pthread_mutex_unlock(&lock);return 0;
}
static int stop(void){pthread_mutex_lock(&lock);release_player();position=0;feedback=NULL;wait_ui();pthread_mutex_unlock(&lock);return 0;}
void cedar_shutdown(void){pthread_mutex_lock(&lock);closing=1;release_player();reap(&waiting);if(touch_fd>=0)close(touch_fd);touch_fd=-1;pthread_mutex_unlock(&lock);free(uri);uri=NULL;}
static int pause_player(void){pthread_mutex_lock(&lock);if(!player){pthread_mutex_unlock(&lock);return -1;}if(!paused){command("pause\n");paused=1;}pthread_mutex_unlock(&lock);return 0;}
static int seek(gint64 ns){pthread_mutex_lock(&lock);char line[80];snprintf(line,sizeof(line),"pausing_keep_force seek %.3f 2\n",(double)ns/1e9);command(line);pthread_mutex_unlock(&lock);return 0;}
static int get_position(gint64 *d,gint64 *p){pthread_mutex_lock(&lock);*d=duration*1e9;*p=position*1e9;pthread_mutex_unlock(&lock);return 0;}
static int set_volume(float v){pthread_mutex_lock(&lock);volume=fmaxf(0,fminf(1,v));char line[64];snprintf(line,sizeof(line),"pausing_keep_force volume %.0f 1\n",muted?0:volume*100);command(line);pthread_mutex_unlock(&lock);return 0;}
static int get_volume(float *v){pthread_mutex_lock(&lock);*v=volume;pthread_mutex_unlock(&lock);return 0;}
static int set_mute(int m){pthread_mutex_lock(&lock);muted=!!m;pthread_mutex_unlock(&lock);return set_volume(volume);}
static int get_mute(int *m){pthread_mutex_lock(&lock);*m=muted;pthread_mutex_unlock(&lock);return 0;}
struct output_module cedar_output={.shortname="cedar",.description="F1C200S Cedar hardware MPlayer",.init=init,.set_uri=set_uri,.play=play,.stop=stop,.pause=pause_player,.seek=seek,.get_position=get_position,.get_volume=get_volume,.set_volume=set_volume,.get_mute=get_mute,.set_mute=set_mute};
