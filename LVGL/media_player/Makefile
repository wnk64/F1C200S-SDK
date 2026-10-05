ROOT := /home/wnk/Desktop/F1c200S_SOFT
HOST := $(ROOT)/ROOTFS/buildroot-2018.02.11/output/host
CC := $(HOST)/bin/arm-buildroot-linux-gnueabi-gcc
LV := $(ROOT)/LVGL/touch/lvgl
CFLAGS := -Os -std=gnu99 -Wall -Wextra -D_GNU_SOURCE -DLV_CONF_INCLUDE_SIMPLE -I. -I$(ROOT)/LVGL/menu -I$(ROOT)/LVGL/touch -I$(LV) -I$(HOST)/arm-buildroot-linux-gnueabi/sysroot/usr/include/drm -ffunction-sections -fdata-sections -MMD -MP
SOURCES := $(wildcard $(LV)/src/*/*.c) $(wildcard $(LV)/src/*/*/*.c) $(wildcard $(LV)/src/*/*/*/*.c)
OBJECTS := $(patsubst $(LV)/%.c,build/%.o,$(SOURCES))
.DEFAULT_GOAL := all
all: mplayer-ui
build/%.o: $(LV)/%.c lv_conf.h
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c -o $@ $<
build/%.o: %.c lv_conf.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<
build/font.o: ../menu/menu_font_stream.c lv_conf.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<
mplayer-ui: build/mplayer_ui.o build/media_display.o build/font.o $(OBJECTS)
	$(CC) -Wl,--gc-sections -o $@ $^ -ldrm -lm
-include build/*.d $(OBJECTS:.o=.d)
