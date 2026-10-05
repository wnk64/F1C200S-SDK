#define _GNU_SOURCE
#include "platform.h"
#include "civetweb.h"
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <unistd.h>
extern const lv_font_t menu_font_stream;
void menu_font_stream_close(void);
static volatile sig_atomic_t quit;
static char ip[INET_ADDRSTRLEN], password[17], authorization[80];
static const char *storage="/root/roms/Uploads";
static int directory_fd=-1;
static void stop(int sig){(void)sig;quit=1;}
static void back(lv_event_t *event){(void)event;quit=1;}
static int reply(struct mg_connection *c,int code,const char *message){mg_send_http_error(c,code,"%s",message);return code;}
static int safe_name(const char *s){
    if(!*s||*s=='.'||strlen(s)>240)return 0;
    for(;*s;s++)if((unsigned char)*s<32||*s=='/'||*s=='\\'||*s==':')return 0;
    return 1;
}
static void json_name(struct mg_connection *c,const char *s){
    mg_printf(c,"\"");for(;*s;s++){if(*s=='"'||*s=='\\')mg_printf(c,"\\%c",*s);else mg_write(c,s,1);}mg_printf(c,"\"");
}
static int request(struct mg_connection *c){
    const struct mg_request_info *r=mg_get_request_info(c);
    const char *auth=mg_get_header(c,"Authorization");
    if(!auth||strcmp(auth,authorization)){
        mg_printf(c,"HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"F1C200S\"\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");return 401;
    }
    if(!strcmp(r->local_uri,"/")&&!strcmp(r->request_method,"GET")){
        mg_send_file(c,"index.html");return 200;
    }
    if(!strcmp(r->local_uri,"/api/files")&&!strcmp(r->request_method,"GET")){
        mg_printf(c,"HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n[");
        DIR *d=fdopendir(openat(directory_fd,".",O_RDONLY|O_DIRECTORY));struct dirent *e;int first=1;
        if(d){while((e=readdir(d))){struct stat s;if(!safe_name(e->d_name)||fstatat(directory_fd,e->d_name,&s,AT_SYMLINK_NOFOLLOW)||!S_ISREG(s.st_mode))continue;
            mg_printf(c,"%s{\"name\":",first?"":",");json_name(c,e->d_name);mg_printf(c,",\"size\":%lld}",(long long)s.st_size);first=0;}closedir(d);}
        mg_printf(c,"]");return 200;
    }
    const char *prefix=!strncmp(r->local_uri,"/api/upload/",12)?"/api/upload/":!strncmp(r->local_uri,"/files/",7)?"/files/":NULL;
    if(!prefix)return reply(c,404,"Not found");
    char name[256];const char *raw=r->local_uri+strlen(prefix);
    int n=mg_url_decode(raw,strlen(raw),name,sizeof(name),0);
    if(n<0||(size_t)n!=strlen(name)||!safe_name(name))return reply(c,400,"Invalid filename");
    if(!strcmp(prefix,"/files/")&&!strcmp(r->request_method,"GET")){
        int fd=openat(directory_fd,name,O_RDONLY|O_NOFOLLOW);struct stat s;
        if(fd<0)return reply(c,404,"File unavailable");
        if(fstat(fd,&s)||!S_ISREG(s.st_mode)){close(fd);return reply(c,403,"Not a regular file");}
        mg_printf(c,"HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %lld\r\nContent-Disposition: attachment\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n",(long long)s.st_size);
        char buffer[32768];ssize_t count;while((count=read(fd,buffer,sizeof(buffer)))>0)if(mg_write(c,buffer,count)!=count)break;
        close(fd);return 200;
    }
    if(strcmp(prefix,"/api/upload/")||strcmp(r->request_method,"PUT"))return reply(c,405,"Method not allowed");
    const char *upload=mg_get_header(c,"X-F1-Upload");
    if(!upload||strcmp(upload,"1"))return reply(c,403,"Upload header required");
    if(r->content_length<0||r->content_length>2147483648LL)return reply(c,413,"Maximum file size is 2 GiB");
    struct stat s;if(!fstatat(directory_fd,name,&s,AT_SYMLINK_NOFOLLOW))return reply(c,409,"File already exists");
    struct statvfs space;if(fstatvfs(directory_fd,&space)||(unsigned long long)space.f_bavail*space.f_frsize<(unsigned long long)r->content_length+1048576)return reply(c,507,"Not enough SD space");
    char temp[80];snprintf(temp,sizeof(temp),".upload-%ld",(long)getpid());
    int fd=openat(directory_fd,temp,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
    if(fd<0)return reply(c,503,"Upload unavailable");
    char buffer[32768];long long left=r->content_length;int failed=0;
    while(left&&!quit){int amount=mg_read(c,buffer,left<(long long)sizeof(buffer)?(size_t)left:sizeof(buffer));
        if(amount<=0){failed=1;break;}int off=0;while(off<amount){ssize_t count=write(fd,buffer+off,amount-off);if(count<=0){failed=1;break;}off+=count;}if(failed)break;left-=amount;}
    if(left||fsync(fd))failed=1;
    close(fd);
    /* One HTTP worker serializes publication; renameat2 forbids external overwrites. */
    if(!failed&&syscall(__NR_renameat2,directory_fd,temp,directory_fd,name,1))failed=1;
    if(failed){unlinkat(directory_fd,temp,0);return reply(c,409,"Upload interrupted or target exists");}
    mg_printf(c,"HTTP/1.1 201 Created\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");return 201;
}
static lv_obj_t *label(const char *value,int y){lv_obj_t *o=lv_label_create(lv_scr_act());lv_obj_set_pos(o,24,y);lv_obj_set_width(o,432);lv_obj_set_style_text_font(o,&menu_font_stream,0);lv_label_set_text(o,value);return o;}
int main(int argc,char **argv){
    int dlna=argc==2&&!strcmp(argv[1],"--dlna");
    struct ifaddrs *addresses;if(getifaddrs(&addresses))return 1;
    for(struct ifaddrs *a=addresses;a;a=a->ifa_next)if(a->ifa_addr&&a->ifa_addr->sa_family==AF_INET&&!strcmp(a->ifa_name,"rtlwlan0"))inet_ntop(AF_INET,&((struct sockaddr_in *)a->ifa_addr)->sin_addr,ip,sizeof(ip));
    freeifaddrs(addresses);if(!*ip){fprintf(stderr,"LAN address unavailable\n");return 1;}
    signal(SIGTERM,stop);signal(SIGINT,stop);
    struct mg_context *server=NULL;
    if(!dlna){
        int random=open("/dev/urandom",O_RDONLY);unsigned char bytes[4];if(random<0||read(random,bytes,4)!=4)return 1;close(random);
        snprintf(password,sizeof(password),"%02x%02x%02x%02x",bytes[0],bytes[1],bytes[2],bytes[3]);
        const char *configured=getenv("F1_WEB_PASSWORD");if(configured&&strlen(configured)>=6&&strlen(configured)<=16)snprintf(password,sizeof(password),"%s",configured);
        char credentials[40];snprintf(credentials,sizeof(credentials),"admin:%s",password);
        static const char digits[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        strcpy(authorization,"Basic ");size_t len=strlen(credentials),out=6;
        for(size_t i=0;i<len;i+=3){unsigned v=(unsigned char)credentials[i]<<16;if(i+1<len)v|=(unsigned char)credentials[i+1]<<8;if(i+2<len)v|=(unsigned char)credentials[i+2];authorization[out++]=digits[v>>18];authorization[out++]=digits[(v>>12)&63];authorization[out++]=i+1<len?digits[(v>>6)&63]:'=';authorization[out++]=i+2<len?digits[v&63]:'=';}authorization[out]=0;
        mkdir(storage,0755);directory_fd=open(storage,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(directory_fd<0)return 1;
        char port[48];snprintf(port,sizeof(port),"%s:8080",ip);
        const char *options[]={"listening_ports",port,"num_threads","1","enable_keep_alive","no","request_timeout_ms","10000","decode_url","no",NULL};
        struct mg_callbacks callbacks={0};callbacks.begin_request=request;
        mg_init_library(0);server=mg_start(&callbacks,NULL,options);if(!server){close(directory_fd);return 1;}
    }
    if(platform_open()){if(server)mg_stop(server);return 2;}
    lv_obj_set_style_bg_color(lv_scr_act(),lv_color_hex(0xf1f4f6),0);
    lv_obj_clear_flag(lv_scr_act(),LV_OBJ_FLAG_SCROLLABLE);
    label(dlna?"DLNA 播放器":"网页传输",40);char address[100];snprintf(address,sizeof(address),"http://%s:%d",ip,dlna?49494:8080);label(address,160);
    if(dlna){label("F1C200S-DLNA",260);label("等待连接",350);}else{char login[100];snprintf(login,sizeof(login),"admin\n%s",password);label(login,280);label("SD / Uploads",430);}
    lv_obj_t *exit=lv_btn_create(lv_scr_act());lv_obj_remove_style_all(exit);
    lv_obj_set_pos(exit,24,650);lv_obj_set_size(exit,432,96);
    lv_obj_set_style_radius(exit,4,0);
    lv_obj_set_style_bg_opa(exit,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(exit,lv_color_white(),0);
    lv_obj_set_style_bg_color(exit,lv_color_hex(0xdceee6),LV_STATE_PRESSED);
    lv_obj_set_style_border_width(exit,2,0);
    lv_obj_set_style_border_opa(exit,LV_OPA_COVER,0);
    lv_obj_set_style_border_color(exit,lv_color_hex(0x52636c),0);
    lv_obj_t *text=lv_label_create(exit);lv_label_set_text(text,LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(text,&lv_font_montserrat_32,0);
    lv_obj_set_style_text_color(text,lv_color_hex(0x172329),0);lv_obj_center(text);
    lv_obj_add_event_cb(exit,back,LV_EVENT_CLICKED,NULL);
    uint64_t tick=platform_ms();const char *limit=getenv("F1_LAN_TEST_MS");uint64_t end=limit?tick+strtoul(limit,NULL,10):0;
    while(!quit){uint64_t now=platform_ms();lv_tick_inc(now-tick);tick=now;lv_timer_handler();if(end&&now>=end)quit=1;usleep(16000);}
    platform_close();menu_font_stream_close();if(server){mg_stop(server);mg_exit_library();close(directory_fd);}return 0;
}
