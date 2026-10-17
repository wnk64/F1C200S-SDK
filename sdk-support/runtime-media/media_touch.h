#ifndef F1_MEDIA_TOUCH_H
#define F1_MEDIA_TOUCH_H

/* LVGL inserts new devices at the head: list order is not device identity. */
static int f1_media_bind_touch(
    void (*portrait)(lv_indev_drv_t *, lv_indev_data_t *),
    void (*landscape)(lv_indev_drv_t *, lv_indev_data_t *))
{
    lv_indev_t *touch = NULL;
    for (lv_indev_t *input = lv_indev_get_next(NULL); input;
         input = lv_indev_get_next(input)) {
        if (input->driver->type != LV_INDEV_TYPE_POINTER ||
            input->driver->read_cb != portrait) continue;
        if (touch) return -1;
        touch = input;
    }
    if (!touch) return -1;
    touch->driver->read_cb = landscape;
    return 0;
}
#endif
