CC := /home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-gcc
STRIP := /home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-strip
LV := ../ft6236_20261002
CFLAGS := -Os -std=gnu99 -Wall -Wextra -DLV_CONF_INCLUDE_SIMPLE -I$(LV) -I$(LV)/lvgl -ffunction-sections -fdata-sections -MMD -MP
LVOBJ := $(patsubst %.c,%.o,$(shell find $(LV)/lvgl/src -name '*.c'))
OBJ := menu.o platform.o menu_font_28.o
.DEFAULT_GOAL := f1-menu
f1-menu: $(OBJ) $(LVOBJ)
	$(CC) -Wl,--gc-sections -o $@ $^ -lm
strip: f1-menu
	$(STRIP) f1-menu
-include $(OBJ:.o=.d)
