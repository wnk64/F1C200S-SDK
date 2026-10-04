ROOT := /home/wnk/Desktop/F1c200S_SOFT
HOST := $(ROOT)/ROOTFS/buildroot-2018.02.11/output/host
CC := $(HOST)/bin/arm-buildroot-linux-gnueabi-gcc
MENU := $(ROOT)/LVGL/menu
LV := $(ROOT)/LVGL/touch/lvgl
SOURCES := $(wildcard $(LV)/src/*/*.c) $(wildcard $(LV)/src/*/*/*.c) $(wildcard $(LV)/src/*/*/*/*.c)
OBJECTS := $(patsubst $(LV)/%.c,build/%.o,$(SOURCES))
CFLAGS := -Os -Wall -Wextra -ffunction-sections -fdata-sections -MMD -MP -D_FILE_OFFSET_BITS=64
CFLAGS += -DLV_CONF_INCLUDE_SIMPLE -I$(MENU) -I$(ROOT)/LVGL/touch -I$(LV)
CFLAGS += -I$(HOST)/arm-buildroot-linux-gnueabi/sysroot/usr/include/freetype2
LIBS := -lpng -ljpeg -lfreetype -lm
all: native-tests ons-native-probe
build/%.o: $(LV)/%.c $(MENU)/lv_conf.h
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c -o $@ $<
build/%.o: %.c native.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<
build/platform.o: $(MENU)/platform.c $(MENU)/platform.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<
native-tests: build/tests.o build/native.o build/archive.o
	$(CC) -Wl,--gc-sections -o $@ $^ $(LIBS)
ons-native-probe: build/probe.o build/native.o build/archive.o build/platform.o $(OBJECTS)
	$(CC) -Wl,--gc-sections -o $@ $^ $(LIBS)
-include $(wildcard build/*.d) $(OBJECTS:.o=.d)
