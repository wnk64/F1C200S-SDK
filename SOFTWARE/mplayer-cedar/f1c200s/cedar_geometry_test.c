#include "cedar_geometry.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    struct cedar_native_view v;
    struct cedar_picture_geometry g = {480,800,480,0,0,480,800};
    assert(!cedar_native_geometry(&g,480,800,&v) && v.width==480 && v.height==800 && !v.x && !v.y && !v.y_offset);
    g=(struct cedar_picture_geometry){384,640,384,8,0,368,640};
    assert(!cedar_native_geometry(&g,480,800,&v) && v.width==360 && v.height==640 && v.x==60 && v.y==80 && v.stride==384 && v.source_x==8 && v.y_tile==0x000f0008);
    g=(struct cedar_picture_geometry){288,480,288,2,0,272,480};
    assert(!cedar_native_geometry(&g,480,800,&v) && v.width==270 && v.x==105 && v.y==160 && v.source_x==2 && v.y_tile==0x000f0002);
    g=(struct cedar_picture_geometry){480,640,480,0,0,480,620};
    assert(!cedar_native_geometry(&g,480,800,&v) && v.height==620 && v.y==90);
    g=(struct cedar_picture_geometry){736,1280,736,8,0,728,1280};
    assert(!cedar_native_geometry(&g,480,800,&v) && v.width==480 && v.height==800 && v.source_x==128 && v.source_y==240 && !v.x && !v.y);
    assert(v.y_offset==168960 && v.uv_offset==74752 && v.y_tile==0x001f1000 && v.uv_tile==0x001f1800);
    g.stride=720; assert(cedar_native_geometry(&g,480,800,&v)==-1);
    g=(struct cedar_picture_geometry){480,640,480,0,0,0,0};
    assert(!cedar_native_geometry(&g,480,800,&v) && v.height==640);
    g.left=1; assert(cedar_native_geometry(&g,480,800,&v)==-1);
    puts("Cedar native geometry PASS"); return 0;
}
