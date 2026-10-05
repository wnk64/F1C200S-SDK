#include "platform.h"
#include "content.h"
#include "document.h"
#include <assert.h>
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
static int is_text, is_document, native, page, image_index, image_count;
static off_t offsets[4096], next_offset;
static char path[PATH_MAX], directory[PATH_MAX], names[256][256], page_text[12288];
static lv_obj_t *body, *title, *status, *previous, *next, *mode_label, *picture;
static lv_obj_t *content_layer, *gesture_layer, *ui_layer;
static int shown, pressed, injected;
static int timer_test_active;
static lv_point_t press_point, injected_point;
static uint64_t pressed_at, ui_activity;
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
    lv_obj_t *o=lv_btn_create(ui_layer);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,72,72);
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
        unsigned w,h;
        pixels=is_document?document_render(page,&w,&h):image_open(path,native,&w,&h);
        if(pixels){
            lv_label_set_text(body,"");memset(&descriptor,0,sizeof(descriptor));descriptor.header.cf=LV_IMG_CF_TRUE_COLOR;
            descriptor.header.w=w;descriptor.header.h=h;descriptor.data_size=w*h*2;descriptor.data=(const uint8_t *)pixels;
            picture=lv_img_create(content_layer);lv_img_set_src(picture,&descriptor);lv_obj_set_pos(picture,(READER_WIDTH-w)/2,(READER_HEIGHT-h)/2);
            lv_obj_clear_flag(picture,LV_OBJ_FLAG_CLICKABLE);
            if(is_document)snprintf(info,sizeof(info),"%d / %d",page+1,document_pages());
            else snprintf(info,sizeof(info),"%d/%d  %s",image_index+1,image_count,image_info());
        }else{lv_label_set_text(body,is_document?document_error():image_error());snprintf(info,sizeof(info),"%d / %d",is_document?page+1:image_index+1,is_document?document_pages():image_count);}
        lv_label_set_text(mode_label,is_document?document_mode():native?"1:1":"Fit");
        set_enabled(previous,is_document?page>0:image_index>0);set_enabled(next,is_document?page+1<document_pages():image_index+1<image_count);
    }
    lv_label_set_text(status,info);
}
static void exit_clicked(lv_event_t *event){(void)event;quit=1;}
static void change_direction(int direction)
{
    if(is_text){if(direction<0){if(!page)return;--page;}else{if(page==4095||next_offset>=text_size()||next_offset<=offsets[page])return;offsets[++page]=next_offset;}}
    else if(is_document){int target=page+direction;if(target<0||target>=document_pages())return;page=target;}
    else{int target=image_index+direction;if(target<0||target>=image_count)return;image_index=target;native=0;
        int n=snprintf(path,sizeof(path),"%s%s%s",directory,!strcmp(directory,"/")?"":"/",names[target]);if(n<0||n>=(int)sizeof(path))return;}
    render();
}
static void change(lv_event_t *event)
{
    ui_activity=platform_ms();
    change_direction((int)(intptr_t)lv_event_get_user_data(event));
}
static void visibility(int visible)
{
    shown=visible;ui_activity=platform_ms();
    if(visible)lv_obj_clear_flag(ui_layer,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ui_layer,LV_OBJ_FLAG_HIDDEN);
}
static int ui_expired(uint64_t now,uint64_t activity)
{
    return now>=activity&&now-activity>=10000;
}
enum gesture_action { GESTURE_NONE, GESTURE_NEXT, GESTURE_PREVIOUS, GESTURE_TOGGLE };
static enum gesture_action classify(lv_point_t from,lv_point_t to,uint64_t elapsed)
{
    int dx=to.x-from.x,dy=to.y-from.y;
    if(elapsed<=1200&&abs(dx)>=64&&abs(dx)>2*abs(dy))
        return dx<0?GESTURE_NEXT:GESTURE_PREVIOUS;
    if(elapsed<=450&&abs(dx)<=16&&abs(dy)<=16&&
        from.x>=120&&from.x<=360&&from.y>=200&&from.y<=600&&
        to.x>=120&&to.x<=360&&to.y>=200&&to.y<=600)return GESTURE_TOGGLE;
    return GESTURE_NONE;
}
static void gesture_event(lv_event_t *event)
{
    if(timer_test_active&&!injected)return;
    lv_event_code_t code=lv_event_get_code(event);
    if(code==LV_EVENT_PRESS_LOST){pressed=0;return;}
    if(code!=LV_EVENT_PRESSED&&code!=LV_EVENT_RELEASED)return;
    lv_point_t point;
    if(injected)point=injected_point;
    else {lv_indev_t *input=lv_indev_get_act();if(!input)return;lv_indev_get_point(input,&point);}
    if(code==LV_EVENT_PRESSED){pressed=1;press_point=point;pressed_at=platform_ms();return;}
    if(!pressed)return;
    pressed=0;
    enum gesture_action action=classify(press_point,point,platform_ms()-pressed_at);
    if(action==GESTURE_TOGGLE)visibility(!shown);
    else if(action==GESTURE_NEXT||action==GESTURE_PREVIOUS){visibility(0);change_direction(action==GESTURE_NEXT?1:-1);}
}
static void test_touch(int x1,int y1,int x2,int y2)
{
    injected=1;injected_point=(lv_point_t){x1,y1};
    lv_event_send(gesture_layer,LV_EVENT_PRESSED,NULL);
    injected_point=(lv_point_t){x2,y2};lv_event_send(gesture_layer,LV_EVENT_RELEASED,NULL);injected=0;
}
static void timer_ui_test(lv_timer_t *timer)
{
    test_touch(240,400,240,400);
    lv_timer_del(timer);
}
static int gesture_selftest(void)
{
    assert(!ui_expired(1000,1001));
    assert(!ui_expired(10999,1000));
    assert(ui_expired(11000,1000));
    assert(classify((lv_point_t){380,400},(lv_point_t){80,410},300)==GESTURE_NEXT);
    assert(classify((lv_point_t){80,400},(lv_point_t){380,390},300)==GESTURE_PREVIOUS);
    assert(classify((lv_point_t){240,400},(lv_point_t){245,405},200)==GESTURE_TOGGLE);
    assert(classify((lv_point_t){240,400},(lv_point_t){240,400},800)==GESTURE_NONE);
    assert(classify((lv_point_t){240,400},(lv_point_t){220,600},300)==GESTURE_NONE);
    assert(classify((lv_point_t){240,400},(lv_point_t){200,405},300)==GESTURE_NONE);
    assert(classify((lv_point_t){10,10},(lv_point_t){10,10},100)==GESTURE_NONE);
    assert(classify((lv_point_t){380,400},(lv_point_t){80,400},2000)==GESTURE_NONE);
    puts("READER gesture classifier PASS");return 0;
}
static lv_obj_t *layer(lv_obj_t *parent,int x,int y,int width,int height)
{
    lv_obj_t *object=lv_obj_create(parent);lv_obj_remove_style_all(object);
    lv_obj_set_pos(object,x,y);lv_obj_set_size(object,width,height);
    lv_obj_clear_flag(object,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);return object;
}
static void mode_clicked(lv_event_t *event)
{
    (void)event;
    ui_activity=platform_ms();
    if(is_text){text_cycle_encoding();page=0;offsets[0]=text_start();}
    else if(is_document){int target=document_resize_text(page);if(target<0){lv_label_set_text(body,document_error());return;}page=target;}
    else native=!native;
    render();
}
int main(int argc,char **argv)
{
    if(argc==2&&!strcmp(argv[1],"--selftest-text")){int rc=text_selftest();puts(rc?"Text encoding FAIL":"Text encoding PASS");return rc;}
    if(argc==2&&!strcmp(argv[1],"--selftest-gestures"))return gesture_selftest();
    if(argc==3&&!strcmp(argv[1],"--check-image")){unsigned w,h;uint16_t *data=image_open(argv[2],0,&w,&h);if(!data){fprintf(stderr,"%s\n",image_error());return 1;}free(data);return 0;}
    if(argc==3&&!strcmp(argv[1],"--check-document")){
        if(document_open(argv[2])){fprintf(stderr,"%s\n",document_error());return 1;}
        int count=document_pages();int result=0;
        for(int i=0;i<(count>1?2:1);++i){unsigned w,h;int number=i?count-1:0;uint16_t *data=document_render(number,&w,&h);
            if(!data){fprintf(stderr,"%s\n",document_error());result=1;break;}
            unsigned checksum=2166136261u;for(unsigned p=0;p<w*h;++p)checksum=(checksum^data[p])*16777619u;
            printf("DOCUMENT page=%d/%d size=%ux%u checksum=%08x\n",number+1,count,w,h,checksum);free(data);}
        document_close();return result;
    }
    if(argc!=2||!realpath(argv[1],path)){fprintf(stderr,"usage: reader file\n");return 1;}
    const char *dot=strrchr(path,'.');is_text=dot&&!strcasecmp(dot,".txt");
    is_document=dot&&(!strcasecmp(dot,".pdf")||!strcasecmp(dot,".epub"));
    if(is_text){if(text_open(path))return 1;offsets[0]=text_start();}else if(is_document)document_open(path);else images_scan();
    if(platform_open()){platform_close();text_close();document_close();return 2;}
    signal(SIGTERM,stop);signal(SIGINT,stop);
    lv_obj_t *root=lv_scr_act();lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(root,is_text||is_document?lv_color_hex(0xf1f4f6):lv_color_black(),0);lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);
    content_layer=layer(root,0,0,READER_WIDTH,READER_HEIGHT);
    body=label(content_layer,"",20,12,440);lv_obj_set_height(body,776);lv_label_set_long_mode(body,LV_LABEL_LONG_WRAP);
    lv_obj_clear_flag(body,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_style_text_line_space(body,0,0);
    if(!is_text&&!is_document)lv_obj_set_style_text_color(body,lv_color_white(),0);
    gesture_layer=layer(root,0,0,READER_WIDTH,READER_HEIGHT);
    lv_obj_add_flag(gesture_layer,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(gesture_layer,gesture_event,LV_EVENT_ALL,NULL);
    ui_layer=layer(root,0,0,READER_WIDTH,READER_HEIGHT);
    lv_obj_t *top_bar=layer(ui_layer,0,0,READER_WIDTH,96),*bottom_bar=layer(ui_layer,0,704,READER_WIDTH,96);
    lv_obj_set_style_bg_opa(top_bar,LV_OPA_COVER,0);lv_obj_set_style_bg_color(top_bar,lv_color_hex(0xf1f4f6),0);
    lv_obj_set_style_bg_opa(bottom_bar,LV_OPA_COVER,0);lv_obj_set_style_bg_color(bottom_bar,lv_color_hex(0xf1f4f6),0);
    lv_obj_t *back=button(16,16,LV_SYMBOL_LEFT);lv_obj_add_event_cb(back,exit_clicked,LV_EVENT_CLICKED,NULL);
    title=label(ui_layer,"",108,34,268);lv_label_set_long_mode(title,LV_LABEL_LONG_DOT);lv_obj_set_height(title,40);
    lv_obj_t *mode=button(392,16,"");mode_label=lv_obj_get_child(mode,0);lv_obj_set_style_text_font(mode_label,&lv_font_montserrat_16,0);lv_obj_add_event_cb(mode,mode_clicked,LV_EVENT_CLICKED,NULL);
    if(is_document&&!document_reflowable())set_enabled(mode,0);
    previous=button(16,712,LV_SYMBOL_LEFT);next=button(392,712,LV_SYMBOL_RIGHT);
    lv_obj_add_event_cb(previous,change,LV_EVENT_CLICKED,(void *)(intptr_t)-1);lv_obj_add_event_cb(next,change,LV_EVENT_CLICKED,(void *)(intptr_t)1);
    status=label(ui_layer,"",98,734,284);lv_obj_set_style_text_font(status,&lv_font_montserrat_16,0);lv_obj_set_style_text_align(status,LV_TEXT_ALIGN_CENTER,0);
    visibility(0);
    const char *show_ui=getenv("READER_TEST_SHOW_UI");if(show_ui&&!strcmp(show_ui,"1"))visibility(1);
    render();uint64_t tick=platform_ms(),started=tick;const char *limit=getenv("READER_TEST_MS");unsigned long test_ms=limit?strtoul(limit,NULL,10):0;
    const char *test=getenv("READER_SELFTEST");int selftest=test&&!strcmp(test,"1"),stage=0;off_t initial=next_offset;
    const char *visibility_test=getenv("READER_TEST_VISIBILITY_ONLY");
    int initial_image=image_index;
    int visibility_checked=0;
    const char *timer_test_env=getenv("READER_TEST_TIMER_UI");
    int timer_test=timer_test_env&&!strcmp(timer_test_env,"1"),timer_seen=0,timer_done=0;
    uint64_t timer_activity=0;
    timer_test_active=timer_test;
    if(timer_test){lv_indev_enable(NULL,false);lv_timer_create(timer_ui_test,250,NULL);}
    while(!quit){uint64_t now=platform_ms();lv_tick_inc(now-tick);tick=now;unsigned delay=lv_timer_handler();if(delay<1)delay=1;if(delay>16)delay=16;
        /* Input callbacks may refresh ui_activity inside lv_timer_handler. */
        now=platform_ms();
        if(shown&&!pressed&&ui_expired(now,ui_activity))visibility(0);
        if(timer_test){
            if(!timer_seen&&now-started>=500){
                if(!shown){fprintf(stderr,"READER timer callback persistence FAIL\n");stage=-1;quit=1;}
                timer_seen=1;timer_activity=ui_activity;
            }
            if(timer_seen&&now-timer_activity<10000&&!shown){fprintf(stderr,"READER timer early hide FAIL age=%llu\n",(unsigned long long)(now-timer_activity));stage=-1;quit=1;}
            if(timer_seen&&now-timer_activity>=10000){
                if(shown){stage=-1;}
                fprintf(stderr,"READER timer callback persistence/10s expiry %s\n",stage<0?"FAIL":"PASS");quit=1;
                timer_done=1;
            }
        }
        if(selftest&&!visibility_checked&&now-started>250){
            uint16_t *before=pixels;
            test_touch(240,400,240,400);
            lv_refr_now(NULL);
            if(!shown||lv_obj_has_flag(ui_layer,LV_OBJ_FLAG_HIDDEN)){stage=-1;quit=1;}
            test_touch(240,400,240,400);
            lv_refr_now(NULL);
            if(shown||!lv_obj_has_flag(ui_layer,LV_OBJ_FLAG_HIDDEN)||pixels!=before){stage=-1;quit=1;}
            visibility_checked=1;fprintf(stderr,"READER center tap show/hide without redecode %s\n",quit?"FAIL":"PASS");
        }
        if(visibility_checked&&visibility_test&&!strcmp(visibility_test,"1"))selftest=0;
        if(selftest&&is_text&&stage==0&&now-started>500){test_touch(380,400,80,400);stage=1;
            if(page!=1||offsets[1]!=initial){fprintf(stderr,"READER next-page FAIL\n");quit=1;stage=-1;}}
        if(selftest&&is_text&&stage==1&&now-started>1000){test_touch(80,400,380,400);stage=2;if(page||next_offset!=initial){fprintf(stderr,"READER previous-page FAIL\n");stage=-1;}else puts("READER swipe page roundtrip PASS");quit=1;}
        if(selftest&&!is_text&&!is_document&&stage==0&&now-started>500){lv_event_send(mode,LV_EVENT_CLICKED,NULL);stage=1;if(!pixels||!native){stage=-1;quit=1;}}
        if(selftest&&!is_text&&!is_document&&stage==1&&now-started>1000){test_touch(380,400,80,400);stage=2;if(!pixels||native||image_index!=initial_image+1){stage=-1;quit=1;}}
        if(selftest&&!is_text&&!is_document&&stage==2&&now-started>1500){test_touch(80,400,380,400);if(!pixels||image_index!=initial_image){stage=-1;}else puts("READER image fit/native/swipe PASS");quit=1;}
        if(selftest&&is_document&&stage==0&&now-started>500){test_touch(380,400,80,400);stage=1;if(page!=1||!pixels){stage=-1;quit=1;}}
        if(selftest&&is_document&&stage==1&&now-started>1000){test_touch(80,400,380,400);stage=2;if(page||!pixels){stage=-1;quit=1;}}
        if(selftest&&is_document&&stage==2&&now-started>1500){if(document_reflowable())lv_event_send(mode,LV_EVENT_CLICKED,NULL);if(!pixels||page<0||page>=document_pages()){stage=-1;}else puts("READER document next/previous/reflow PASS");quit=1;}
        if(test_ms&&now-started>=test_ms)quit=1;
        usleep(delay*1000);}
    if(picture)lv_obj_del(picture);
    free(pixels);text_close();document_close();platform_close();menu_font_stream_close();return stage<0||(timer_test&&!timer_done)?1:0;
}
