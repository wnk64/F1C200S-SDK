#include "platform.h"
#include "content.h"
#include <dirent.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
extern const lv_font_t reader_font_28;
void menu_font_stream_close(void);
static volatile sig_atomic_t quit;
static int is_text, native, page, image_index, image_count;
static off_t offsets[4096], next_offset;
static char path[PATH_MAX], directory[PATH_MAX], names[256][256], page_text[12288];
static lv_obj_t *body, *title, *status, *previous, *next, *mode_label, *picture;
static uint16_t *pixels;
static lv_img_dsc_t descriptor;
static void stop(int signal){(void)signal;quit=1;}
static lv_obj_t *label(lv_obj_t *parent,const char *value,int x,int y,int width)
{
    lv_obj_t *o=lv_label_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,width);
    lv_obj_set_style_text_font(o,&reader_font_28,0);lv_obj_set_style_text_color(o,lv_color_hex(0x172329),0);
    lv_label_set_text(o,value);return o;
}
static lv_obj_t *button(int x,int y,const char *symbol)
{
    lv_obj_t *o=lv_btn_create(lv_scr_act());lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,72,72);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_bg_color(o,lv_color_white(),0);
    lv_obj_set_style_bg_color(o,lv_color_hex(0xd5dfe3),LV_STATE_PRESSED);lv_obj_set_style_radius(o,4,0);
    lv_obj_t *text=label(o,symbol,0,0,72);lv_obj_set_style_text_font(text,&lv_font_montserrat_32,0);lv_obj_set_style_text_align(text,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(text);return o;
}
static void set_enabled(lv_obj_t *object,int enabled){if(enabled)lv_obj_clear_state(object,LV_STATE_DISABLED);else lv_obj_add_state(object,LV_STATE_DISABLED);}
static int image_name(const char *name)
{
    const char *dot=strrchr(name,'.');const char *types[]={".jpg",".jpeg",".png",".bmp",".gif",".tga",".ppm",".pgm",".pnm",".psd"};
    for(unsigned i=0;dot&&i<sizeof(types)/sizeof(*types);++i)if(!strcasecmp(dot,types[i]))return 1;
    return 0;
}
static int compare(const void *a,const void *b){return strcmp(a,b);}
static void images_scan(void)
{
    strcpy(directory,path);char *slash=strrchr(directory,'/');if(!slash)return;
    if(slash==directory)slash[1]=0;else *slash=0;
    DIR *dir=opendir(directory);if(!dir)return;struct dirent *item;
    while((item=readdir(dir))&&image_count<256){
        if(!image_name(item->d_name)||strchr(item->d_name,'\n')||strlen(item->d_name)>=256)continue;
        char candidate[PATH_MAX];struct stat st;
        int n=snprintf(candidate,sizeof(candidate),"%s/%s",directory,item->d_name);
        if(n<0||n>=(int)sizeof(candidate)||stat(candidate,&st)||!S_ISREG(st.st_mode))continue;
        strcpy(names[image_count++],item->d_name);
    }
    closedir(dir);qsort(names,image_count,sizeof(names[0]),compare);
    const char *name=strrchr(path,'/');name=name?name+1:path;
    for(int i=0;i<image_count;++i)if(!strcmp(names[i],name)){image_index=i;break;}
}
static void render(void)
{
    const char *name=strrchr(path,'/');lv_label_set_text(title,name?name+1:path);
    char info[160];
    if(is_text){
        if(text_page(offsets[page],page_text,sizeof(page_text),&next_offset))strcpy(page_text,"读取失败");
        lv_label_set_text(body,page_text);
        snprintf(info,sizeof(info),"%d  |  %.1f%%",page+1,text_size()?100.0*(double)next_offset/text_size():100.0);
        lv_label_set_text(mode_label,text_encoding());
        set_enabled(previous,page>0);set_enabled(next,next_offset<text_size()&&next_offset>offsets[page]&&page<4095);
        fprintf(stderr,"READER text page=%d bytes=%ld..%ld encoding=%s\n",page+1,(long)offsets[page],(long)next_offset,text_encoding());
    }else{
        if(picture){lv_obj_del(picture);picture=NULL;}free(pixels);pixels=NULL;
        unsigned w,h;pixels=image_open(path,native,&w,&h);
        if(pixels){
            lv_label_set_text(body,"");memset(&descriptor,0,sizeof(descriptor));descriptor.header.cf=LV_IMG_CF_TRUE_COLOR;
            descriptor.header.w=w;descriptor.header.h=h;descriptor.data_size=w*h*2;descriptor.data=(const uint8_t *)pixels;
            picture=lv_img_create(lv_scr_act());lv_img_set_src(picture,&descriptor);lv_obj_set_pos(picture,(480-w)/2,104+(600-h)/2);
            snprintf(info,sizeof(info),"%d/%d  %s",image_index+1,image_count,image_info());
        }else{lv_label_set_text(body,image_error());snprintf(info,sizeof(info),"%d / %d",image_index+1,image_count);}
        lv_label_set_text(mode_label,native?"1:1":"Fit");set_enabled(previous,image_index>0);set_enabled(next,image_index+1<image_count);
    }
    lv_label_set_text(status,info);
}
static void exit_clicked(lv_event_t *event){(void)event;quit=1;}
static void change(lv_event_t *event)
{
    int direction=(int)(intptr_t)lv_event_get_user_data(event);
    if(is_text){if(direction<0){if(!page)return;--page;}else{if(page==4095||next_offset>=text_size()||next_offset<=offsets[page])return;offsets[++page]=next_offset;}}
    else{int target=image_index+direction;if(target<0||target>=image_count)return;image_index=target;
        int n=snprintf(path,sizeof(path),"%s%s%s",directory,!strcmp(directory,"/")?"":"/",names[target]);if(n<0||n>=(int)sizeof(path))return;}
    render();
}
static void mode_clicked(lv_event_t *event)
{
    (void)event;if(is_text){text_cycle_encoding();page=0;offsets[0]=text_start();}else native=!native;render();
}
int main(int argc,char **argv)
{
    if(argc==2&&!strcmp(argv[1],"--selftest-text")){int rc=text_selftest();puts(rc?"Text encoding FAIL":"Text encoding PASS");return rc;}
    if(argc==3&&!strcmp(argv[1],"--check-image")){unsigned w,h;uint16_t *data=image_open(argv[2],0,&w,&h);if(!data){fprintf(stderr,"%s\n",image_error());return 1;}free(data);return 0;}
    if(argc!=2||!realpath(argv[1],path)){fprintf(stderr,"usage: reader file\n");return 1;}
    const char *dot=strrchr(path,'.');is_text=dot&&!strcasecmp(dot,".txt");
    if(is_text){if(text_open(path))return 1;offsets[0]=text_start();}else images_scan();
    if(platform_open()){platform_close();text_close();return 2;}
    signal(SIGTERM,stop);signal(SIGINT,stop);
    lv_obj_t *root=lv_scr_act();lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(root,lv_color_hex(0xf1f4f6),0);lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);
    lv_obj_t *back=button(16,16,LV_SYMBOL_LEFT);lv_obj_add_event_cb(back,exit_clicked,LV_EVENT_CLICKED,NULL);
    title=label(root,"",108,34,268);lv_label_set_long_mode(title,LV_LABEL_LONG_DOT);lv_obj_set_height(title,40);
    lv_obj_t *mode=button(392,16,"");mode_label=lv_obj_get_child(mode,0);lv_obj_set_style_text_font(mode_label,&lv_font_montserrat_16,0);lv_obj_add_event_cb(mode,mode_clicked,LV_EVENT_CLICKED,NULL);
    body=label(root,"",20,104,440);lv_obj_set_height(body,600);lv_label_set_long_mode(body,LV_LABEL_LONG_WRAP);
    previous=button(16,712,LV_SYMBOL_LEFT);next=button(392,712,LV_SYMBOL_RIGHT);
    lv_obj_add_event_cb(previous,change,LV_EVENT_CLICKED,(void *)(intptr_t)-1);lv_obj_add_event_cb(next,change,LV_EVENT_CLICKED,(void *)(intptr_t)1);
    status=label(root,"",98,734,284);lv_obj_set_style_text_font(status,&lv_font_montserrat_16,0);lv_obj_set_style_text_align(status,LV_TEXT_ALIGN_CENTER,0);
    render();uint64_t tick=platform_ms(),started=tick;const char *limit=getenv("READER_TEST_MS");unsigned long test_ms=limit?strtoul(limit,NULL,10):0;
    const char *test=getenv("READER_SELFTEST");int selftest=test&&!strcmp(test,"1"),stage=0;off_t initial=next_offset;
    int initial_image=image_index;
    while(!quit){uint64_t now=platform_ms();lv_tick_inc(now-tick);tick=now;unsigned delay=lv_timer_handler();if(delay<1)delay=1;if(delay>16)delay=16;
        if(selftest&&is_text&&stage==0&&now-started>500){lv_event_send(next,LV_EVENT_CLICKED,NULL);stage=1;
            if(page!=1||offsets[1]!=initial){fprintf(stderr,"READER next-page FAIL\n");quit=1;stage=-1;}}
        if(selftest&&is_text&&stage==1&&now-started>1000){lv_event_send(previous,LV_EVENT_CLICKED,NULL);stage=2;if(page||next_offset!=initial){fprintf(stderr,"READER previous-page FAIL\n");stage=-1;}else puts("READER page roundtrip PASS");quit=1;}
        if(selftest&&!is_text&&stage==0&&now-started>500){lv_event_send(mode,LV_EVENT_CLICKED,NULL);stage=1;if(!pixels||!native){stage=-1;quit=1;}}
        if(selftest&&!is_text&&stage==1&&now-started>1000){lv_event_send(mode,LV_EVENT_CLICKED,NULL);lv_event_send(next,LV_EVENT_CLICKED,NULL);stage=2;if(!pixels||image_index!=initial_image+1){stage=-1;quit=1;}}
        if(selftest&&!is_text&&stage==2&&now-started>1500){lv_event_send(previous,LV_EVENT_CLICKED,NULL);if(!pixels||image_index!=initial_image){stage=-1;}else puts("READER image fit/native/next/previous PASS");quit=1;}
        if(test_ms&&now-started>=test_ms)quit=1;
        usleep(delay*1000);}
    if(picture)lv_obj_del(picture);
    free(pixels);text_close();platform_close();menu_font_stream_close();return stage<0?1:0;
}
