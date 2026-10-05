ROOT := /home/wnk/Desktop/F1c200S_SOFT
HOST := $(ROOT)/ROOTFS/buildroot-2018.02.11/output/host
CC := $(HOST)/bin/arm-buildroot-linux-gnueabi-gcc
LV := $(ROOT)/LVGL/touch/lvgl
CFLAGS := -Os -std=gnu99 -Wall -Wextra -D_GNU_SOURCE -DLV_CONF_INCLUDE_SIMPLE -I. -I$(ROOT)/LVGL/menu -I$(ROOT)/LVGL/touch -I$(LV) -ffunction-sections -fdata-sections -MMD -MP
SOURCES := $(wildcard $(LV)/src/*/*.c) $(wildcard $(LV)/src/*/*/*.c) $(wildcard $(LV)/src/*/*/*/*.c)
OBJECTS := $(patsubst $(LV)/%.c,build/%.o,$(SOURCES))
.DEFAULT_GOAL := all
all: reader reader-browser
build/%.o: $(LV)/%.c lv_conf.h
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c -o $@ $<
build/%.o: %.c lv_conf.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<
build/platform.o: ../menu/platform.c lv_conf.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<
build/font.o: ../menu/menu_font_stream.c lv_conf.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<
reader: build/reader.o build/image.o build/text.o build/reader_font_28.o build/platform.o build/font.o $(OBJECTS)
	$(CC) -Wl,--gc-sections -o $@ $^ -lm
reader-browser: build/reader_browser.o build/platform.o build/font.o $(OBJECTS)
	$(CC) -Wl,--gc-sections -o $@ $^ -lm
-include build/*.d $(OBJECTS:.o=.d)
