#ifndef MENU_SYSTEM_STATUS_H
#define MENU_SYSTEM_STATUS_H
#include <stddef.h>
void system_ssh_address(char *text, size_t length);
void system_battery(char *text, size_t length, int *charging);
#endif
