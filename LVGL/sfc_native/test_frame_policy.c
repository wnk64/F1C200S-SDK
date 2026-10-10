#include "frame_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    for(unsigned skip=0;skip<=5;skip++){
        unsigned skipped=0,draws=0;
        for(unsigned frame=0;frame<1000;frame++){
            assert(sfc_render_due(frame,0,skip,100000,16667,0)==(frame%(skip+1)==0));
            assert(sfc_render_due(frame,frame?(frame-1)%(skip+1):0,skip,0,16667,1)==(frame%(skip+1)==0));
            if(sfc_render_due(frame,skipped,skip,100000,16667,1)){skipped=0;draws++;}
            else skipped++;
            assert(skipped<=skip+1);
        }
        assert(draws>0);
    }
    assert(sfc_render_due(2,1,1,8333,16667,1));
    assert(!sfc_render_due(2,1,1,8334,16667,1));
    assert(sfc_render_due(3,2,1,100000,16667,1));
    puts("SFC fixed/adaptive render timing and maximum skip bound PASS");
    return 0;
}
