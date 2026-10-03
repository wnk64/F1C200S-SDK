CC := /home/wnk/Desktop/F1c200S_SOFT/ROOTFS/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-gcc
STRIP := /home/wnk/Desktop/F1c200S_SOFT/ROOTFS/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-strip
LV := ../touch
CFLAGS := -Os -std=gnu99 -Wall -Wextra -DLV_CONF_INCLUDE_SIMPLE -I. -I$(LV) -I$(LV)/lvgl -ffunction-sections -fdata-sections -MMD -MP
LVOBJ := $(patsubst $(LV)/lvgl/%.c,build/lvgl/%.o,$(shell find $(LV)/lvgl/src -name '*.c'))
OBJ := menu.o platform.o menu_font_28.o menu_font_stream.o connections.o net_protocol.o time-settings.o system-status.o ip-settings.o
SYSROOT := $(shell $(CC) --print-sysroot)
.DEFAULT_GOAL := all
all: f1-menu connections-helper
cast-wait-touch: cast-wait-touch.c ../touch/touch_orientation.h
	$(CC) -Os -std=gnu99 -Wall -Wextra -Werror -o $@ $<
$(OBJ): lv_conf.h
build/lvgl/%.o: $(LV)/lvgl/%.c lv_conf.h
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $<
f1-menu: $(OBJ) $(LVOBJ)
	$(CC) -Wl,--gc-sections -o $@ $^ -lm
connections-helper.o: connections-helper.c net_protocol.h
	$(CC) -Os -std=gnu99 -Wall -Wextra -I$(SYSROOT)/usr/include/dbus-1.0 -I$(SYSROOT)/usr/lib/dbus-1.0/include -c -o $@ $<
connections-helper: connections-helper.o net_protocol.o
	$(CC) -Wl,--gc-sections -o $@ $^ -ldbus-1
strip: all
	$(STRIP) f1-menu
	$(STRIP) connections-helper
-include $(OBJ:.o=.d) $(LVOBJ:.o=.d)
