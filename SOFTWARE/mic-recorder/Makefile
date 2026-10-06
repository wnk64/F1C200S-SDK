ROOT := /home/wnk/Desktop/F1c200S_SOFT
MENU := $(ROOT)/LVGL/menu
CC := $(ROOT)/ROOTFS/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-gcc
CFLAGS := -Os -std=gnu99 -Wall -Wextra -DLV_CONF_INCLUDE_SIMPLE -I$(MENU) -I$(ROOT)/LVGL/touch -I$(ROOT)/LVGL/touch/lvgl
all: mic-recorder
mic-recorder: recorder.c
	$(MAKE) -C $(MENU) f1-menu
	$(CC) $(CFLAGS) -Wl,--gc-sections -o $@ $< $(MENU)/platform.o $(MENU)/menu_font_stream.o $$(find $(MENU)/build/lvgl -name '*.o') -lasound -lm
