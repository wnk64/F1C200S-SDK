#include <stdint.h>
#include <stdio.h>
#include <string.h>
static void le32(uint8_t *p,uint32_t v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
int main(int argc,char **argv)
{
    if(argc!=2)return 1;
    uint8_t exe[4096]={0};memcpy(exe,"PS-X EXE",8);le32(exe+0x10,0x80010000);le32(exe+0x18,0x80010000);le32(exe+0x1c,2048);le32(exe+0x30,0x801fff00);
    /* Self-authored R3000 program: configure GPU and vary full-screen fill. */
    const uint32_t code[]={0x3c081f80,0x35081810,0xad000004,
        0x3c090300,0xad090004,0x3c090500,0xad090004,
        0x3c0906c6,0x35290260,0xad090004,0x3c090704,0x35292018,0xad090004,
        0x3c090800,0x35290001,0xad090004,0x240a0000,
        0x254a0001,0x314b00ff,0x3c090200,0x012b4825,0xad090000,0xad000000,
        0x3c0900f0,0x35290140,0xad090000,0x1000fff6,0x00000000};
    for(unsigned i=0;i<sizeof(code)/4;i++)le32(exe+2048+i*4,code[i]);
    FILE *f=fopen(argv[1],"wb");if(!f)return 1;int okay=fwrite(exe,1,sizeof(exe),f)==sizeof(exe);if(fclose(f))okay=0;return !okay;
}
