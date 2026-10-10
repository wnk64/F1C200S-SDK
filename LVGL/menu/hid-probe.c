#define _POSIX_C_SOURCE 200809L
#include "evdev-hid.h"
#include <stdlib.h>
#include <time.h>
static uint64_t now_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
int main(int argc,char **argv)
{
    unsigned seconds=argc>1?strtoul(argv[1],NULL,10):15;if(!seconds||seconds>60)return 1;
    f1_hid state;f1_hid_init(&state);uint64_t end=now_ms()+seconds*1000;unsigned previous=0;int device_count=0,changes=0;
    while(now_ms()<end){f1_hid_poll(&state,now_ms(),480,800);
        int count=0;for(int i=0;i<F1_HID_DEVICES;i++)if(state.devices[i].fd>=0)count++;
        if(count!=device_count){printf("devices=%d\n",count);device_count=count;}
        unsigned pad=f1_hid_pad(&state);if(pad!=previous){printf("pad=%04x\n",pad);previous=pad;changes++;}
        while(state.kn){f1_hid_key key=state.keys[state.kh];state.kh=(state.kh+1)%F1_HID_EVENTS;state.kn--;printf("key=%u down=%d\n",key.key,key.down);}
        state.pn=0;fflush(stdout);struct timespec delay={.tv_nsec=8000000};nanosleep(&delay,NULL);
    }
    f1_hid_close(&state);printf("changes=%d\n",changes);return 0;
}
