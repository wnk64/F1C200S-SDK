#include <stdint.h>
#include <stdio.h>
#include <string.h>
static void be32(uint8_t *p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
int main(int argc,char **argv)
{
    if(argc!=2)return 1;uint8_t rom[65536]={0};be32(rom,0x00ffff00);be32(rom+4,0x200);
    memcpy(rom+0x100,"SEGA MEGA DRIVE ",16);memcpy(rom+0x120,"NATIVE FRONTEND SELF TEST",25);
    memcpy(rom+0x150,"NATIVE FRONTEND SELF TEST",25);be32(rom+0x1a4,sizeof(rom)-1);be32(rom+0x1a8,0xff0000);be32(rom+0x1ac,0xffffff);memcpy(rom+0x1f0,"JUE",3);
    /* Self-authored 68000 program: display-enable, repeatedly update CRAM0. */
    const uint16_t code[]={0x46fc,0x2700,0x33fc,0x8004,0x00c0,0x0004,0x33fc,0x8174,0x00c0,0x0004,
        0x7000,0x23fc,0xc000,0x0000,0x00c0,0x0004,0x33c0,0x00c0,0x0000,0x5240,0x60ec};
    for(unsigned i=0;i<sizeof(code)/2;i++){rom[0x200+i*2]=code[i]>>8;rom[0x201+i*2]=code[i];}
    unsigned sum=0;for(unsigned i=0x200;i<sizeof(rom);i+=2)sum+=(rom[i]<<8)|rom[i+1];rom[0x18e]=sum>>8;rom[0x18f]=sum;
    FILE *f=fopen(argv[1],"wb");if(!f)return 1;int okay=fwrite(rom,1,sizeof(rom),f)==sizeof(rom);if(fclose(f))okay=0;return !okay;
}
