ROOT := /home/wnk/Desktop/F1c200S_SOFT
HOST := $(ROOT)/ROOTFS/buildroot-2018.02.11/output/host
CC := $(HOST)/bin/arm-buildroot-linux-gnueabi-gcc
MENU := $(ROOT)/LVGL/menu
CFLAGS := -O2 -march=armv5te -mtune=arm926ej-s -marm -msoft-float -D_GNU_SOURCE -DLV_CONF_INCLUDE_SIMPLE -Wall -Wextra -Wno-unused-function -ffunction-sections -fdata-sections -MMD -MP -I. -Ivendor/pcsx/deps/libretro-common/include -I$(MENU) -I$(ROOT)/LVGL/touch -I$(ROOT)/LVGL/touch/lvgl
.PHONY: all platform
all: retro-native retro-browser
platform:
	$(MAKE) -C $(MENU) f1-menu
build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
retro-native: build/frontend.o build/display.o platform
	$(CC) -Wl,--gc-sections -o $@ build/frontend.o build/display.o $(MENU)/menu_font_stream.o $$(find $(MENU)/build/lvgl -name '*.o') -lasound -ldl -lm
retro-browser: build/rom_browser.o platform
	$(CC) -Wl,--gc-sections -o $@ build/rom_browser.o $(MENU)/platform.o $(MENU)/menu_font_stream.o $$(find $(MENU)/build/lvgl -name '*.o') -lm
-include build/*.d
