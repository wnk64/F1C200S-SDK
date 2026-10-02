#ifndef FT_TOUCH_ORIENTATION_H
#define FT_TOUCH_ORIENTATION_H
static inline void ft_touch_portrait(int raw_x, int raw_y, int *x, int *y)
{
    *x = 479 - raw_y;
    *y = raw_x;
}
#endif
