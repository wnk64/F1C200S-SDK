#ifndef MENU_UI_H
#define MENU_UI_H
#include "platform.h"
lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int width, uint32_t color);
lv_obj_t *button(lv_obj_t *parent, int x, int y, int width, int height, uint32_t color);
void header(const char *title, int can_back);
lv_obj_t *menu_content(void);
lv_obj_t *menu_status(void);
void menu_back_to(void (*callback)(void));
void menu_settings(void);
#endif
