ROOT := /home/wnk/Desktop/F1c200S_SOFT
HOST := $(ROOT)/ROOTFS/buildroot-2018.02.11/output/host
CC := $(HOST)/bin/arm-buildroot-linux-gnueabi-gcc
LV := $(ROOT)/LVGL/touch/lvgl
MENU := $(ROOT)/LVGL/menu
CFLAGS := -O2 -march=armv5te -mtune=arm926ej-s -marm -msoft-float -fno-strict-aliasing -ffunction-sections -fdata-sections -MMD -MP -D_GNU_SOURCE -DRIGHTSHIFT_IS_SAR -DNO_ZERO_LUT -DLV_CONF_INCLUDE_SIMPLE -I. -Ivendor/core -I$(MENU) -I$(ROOT)/LVGL/touch -I$(LV)
CORE := $(patsubst vendor/core/%.c,build/core/%.o,$(wildcard vendor/core/*.c))
.PHONY: all platform
all: sfc-native sfc-browser
platform:
	$(MAKE) -C $(MENU) f1-menu
build/core/%.o: vendor/core/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -include stddef.h -include stdio.h -include stdint.h -c $< -o $@
build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -Wall -Wextra -Wno-unused-function -c $< -o $@
sfc-native: build/frontend.o build/display.o $(CORE) platform
	$(CC) -Wl,--gc-sections -o $@ build/frontend.o build/display.o $(CORE) $(MENU)/menu_font_stream.o $$(find $(MENU)/build/lvgl -name '*.o') -lasound -lm -lz
sfc-browser: build/rom_browser.o platform
	$(CC) -Wl,--gc-sections -o $@ build/rom_browser.o $(MENU)/platform.o $(MENU)/menu_font_stream.o $$(find $(MENU)/build/lvgl -name '*.o') -lm
-include build/*.d build/core/*.d
