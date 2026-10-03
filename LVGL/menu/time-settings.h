#ifndef MENU_TIME_SETTINGS_H
#define MENU_TIME_SETTINGS_H
#include <stddef.h>
void time_settings_show(void);
void time_settings_poll(void);
void time_settings_close(void);
int time_settings_test_step(void);
void time_settings_format_clock(char *text, size_t length);
#endif
