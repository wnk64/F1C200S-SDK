#include "touch_orientation.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    const int points[][4] = {{171,402,77,171},{198,90,389,198},
        {721,368,111,721},{707,62,417,707},
        {0,0,479,0},{799,0,479,799},{0,479,0,0},{799,479,0,799}};
    for (unsigned i = 0; i < sizeof(points)/sizeof(points[0]); i++) {
        int x, y; ft_touch_portrait(points[i][0], points[i][1], &x, &y);
        assert(x == points[i][2] && y == points[i][3]);
        assert(x >= 0 && x < 480 && y >= 0 && y < 800);
    }
    puts("Four measured points and four native corners passed");
    return 0;
}
