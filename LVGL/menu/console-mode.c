#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <linux/kd.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc,char **argv)
{
    if(argc!=2){fprintf(stderr,"usage: f1-console-mode status|graphics|0|1\n");return 1;}
    int target;
    if(!strcmp(argv[1],"status"))target=-1;
    else if(!strcmp(argv[1],"graphics")||!strcmp(argv[1],"1"))target=KD_GRAPHICS;
    else if(!strcmp(argv[1],"0"))target=KD_TEXT;
    else return 1;
    int fd=open("/dev/tty0",O_RDWR|O_NOCTTY|O_CLOEXEC),mode;
    if(fd<0){perror("console open");return 1;}
    if(ioctl(fd,KDGETMODE,&mode)|| (target>=0&&ioctl(fd,KDSETMODE,target))){
        perror("console mode");close(fd);return 1;
    }
    close(fd);
    /* Return the prior mode so the supervisor can restore it on shutdown. */
    printf("%d\n",mode);
    return 0;
}
