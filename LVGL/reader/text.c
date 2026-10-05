#include "platform.h"
#include "content.h"
#include "gbk_map.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
extern const lv_font_t menu_font_stream;
static int file_fd = -1, encoding;
static off_t size, beginning;
enum { UTF8, GBK, UTF16LE, UTF16BE };
static int utf8_code(const unsigned char *s, unsigned n, unsigned *code)
{
    if (!n) return 0;
    if (s[0] < 128) { *code = s[0]; return 1; }
    int length = s[0]>=0xc2 && s[0]<=0xdf ? 2 : s[0]>=0xe0 && s[0]<=0xef ? 3 : s[0]>=0xf0 && s[0]<=0xf4 ? 4 : -1;
    if (length < 0) return -1; if ((unsigned)length > n) return 0;
    unsigned value = s[0] & (0x7f >> length);
    for (int i = 1; i < length; ++i) { if ((s[i]&0xc0)!=0x80) return -1; value=(value<<6)|(s[i]&63); }
    if ((length==2 && value<128)||(length==3 && value<2048)||(length==4 && value<65536)||value>0x10ffff||(value>=0xd800 && value<=0xdfff)) return -1;
    *code=value; return length;
}
static int decode(const unsigned char *s, unsigned n, unsigned *code)
{
    if (encoding==UTF8) return utf8_code(s,n,code);
    if (!n) return 0;
    if (encoding==GBK) {
        if (*s<128) { *code=*s; return 1; }
        if (n<2) return 0;
        if (s[0]<0x81||s[0]>0xfe||s[1]<0x40||s[1]>0xfe||s[1]==0x7f) return -1;
        unsigned slot=(s[0]-0x81)*190+s[1]-0x40-(s[1]>0x7f); *code=gbk_map[slot]; return *code?2:-1;
    }
    if(n<2)return 0;
    unsigned first=encoding==UTF16LE ? s[0]|s[1]<<8 : s[0]<<8|s[1];
    if(first>=0xd800&&first<=0xdbff){
        if(n<4)return 0;
        unsigned second=encoding==UTF16LE?s[2]|s[3]<<8:s[2]<<8|s[3];
        if(second<0xdc00||second>0xdfff)return -1;
        *code=0x10000+((first-0xd800)<<10)+second-0xdc00;return 4;
    }
    if(first>=0xdc00&&first<=0xdfff)return -1;
    *code=first; return 2;
}
static unsigned encode(char *s,unsigned code)
{
    if(code<128){s[0]=code;return 1;}
    if(code<2048){s[0]=0xc0|(code>>6);s[1]=0x80|(code&63);return 2;}
    if(code<65536){s[0]=0xe0|(code>>12);s[1]=0x80|((code>>6)&63);s[2]=0x80|(code&63);return 3;}
    s[0]=0xf0|(code>>18);s[1]=0x80|((code>>12)&63);s[2]=0x80|((code>>6)&63);s[3]=0x80|(code&63);return 4;
}
void text_close(void){if(file_fd>=0)close(file_fd);file_fd=-1;}
off_t text_start(void){return beginning;}
off_t text_size(void){return size;}
const char *text_encoding(void){return (const char *[]) {"UTF-8","GBK","UTF-16LE","UTF-16BE"}[encoding];}
void text_cycle_encoding(void){encoding=(encoding+1)%4;}
int text_open(const char *path)
{
    text_close();file_fd=open(path,O_RDONLY|O_CLOEXEC);if(file_fd<0)return -1;
    struct stat st;if(fstat(file_fd,&st)||!S_ISREG(st.st_mode)){text_close();return -1;}size=st.st_size;beginning=0;encoding=UTF8;
    unsigned char sample[4096];ssize_t n=pread(file_fd,sample,sizeof(sample),0);if(n<0){text_close();return -1;}
    if(n>=3&&!memcmp(sample,"\xef\xbb\xbf",3))beginning=3;
    else if(n>=2&&sample[0]==255&&sample[1]==254){encoding=UTF16LE;beginning=2;}
    else if(n>=2&&sample[0]==254&&sample[1]==255){encoding=UTF16BE;beginning=2;}
    else for(unsigned i=0;i<(unsigned)n;){unsigned code;int used=utf8_code(sample+i,n-i,&code);if(used<0){encoding=GBK;break;}if(!used)break;i+=used;}
    return 0;
}
int text_page(off_t start,char *output,size_t capacity,off_t *end)
{
    unsigned char raw[4096];unsigned map[12288];size_t out=0;unsigned consumed=0;
    ssize_t n=pread(file_fd,raw,sizeof(raw),start);if(n<0)return -1;
    while(consumed<(unsigned)n&&out+5<capacity&&out+5<sizeof(map)/sizeof(*map)){
        unsigned code;int used=decode(raw+consumed,n-consumed,&code);if(!used)break;
        if(used<0){code=0xfffd;used=encoding>=UTF16LE&&n-consumed>=2?2:1;}
        consumed+=used;
        if(code=='\r')continue;
        if(code=='\t')code=' ';if(!code)code=0xfffd;
        unsigned bytes=encode(output+out,code);for(unsigned i=0;i<bytes;++i)map[out+i]=consumed-used;
        out+=bytes;map[out]=consumed;
    }
    output[out]=0;unsigned cut=0;
    for(unsigned line=0;line<16&&cut<out;++line){unsigned bytes=_lv_txt_get_next_line(output+cut,&menu_font_stream,0,440,NULL,LV_TEXT_FLAG_NONE);if(!bytes)break;cut+=bytes;}
    if(cut>out)cut=out;*end=start+(cut?map[cut]:consumed);output[cut]=0;return 0;
}
int text_selftest(void)
{
    unsigned code;assert(utf8_code((unsigned char *)"\xe4\xb8\xad",3,&code)==3&&code==0x4e2d);
    assert(utf8_code((unsigned char *)"\xc0\x80",2,&code)==-1);
    encoding=GBK;assert(decode((unsigned char *)"\xd6\xd0",2,&code)==2&&code==0x4e2d);
    encoding=UTF16LE;assert(decode((unsigned char *)"\x2d\x4e",2,&code)==2&&code==0x4e2d);
    encoding=UTF16BE;assert(decode((unsigned char *)"\x4e\x2d",2,&code)==2&&code==0x4e2d);
    return 0;
}
