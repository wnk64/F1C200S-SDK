ROOT := /home/wnk/Desktop/F1c200S_SOFT
CC := $(ROOT)/ROOTFS/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-gcc
MENU := $(ROOT)/LVGL/menu
LV := $(ROOT)/LVGL/touch/lvgl
SOURCES := $(wildcard $(LV)/src/*/*.c) $(wildcard $(LV)/src/*/*/*.c) $(wildcard $(LV)/src/*/*/*/*.c)
OBJECTS := $(patsubst $(LV)/%.c,build/%.o,$(SOURCES))
CFLAGS := -Os -fPIC -ffunction-sections -fdata-sections -DLV_CONF_INCLUDE_SIMPLE -I$(MENU) -I$(ROOT)/LVGL/touch -I$(LV) -MMD -MP
all: libons-lvgl.so
build/%.o: $(LV)/%.c $(MENU)/lv_conf.h
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c -o $@ $<
build/platform.o: $(MENU)/platform.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<
build/backend.o: ons_lvgl_backend.c touch_events.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -Wall -Wextra -c -o $@ $<
libons-lvgl.so: build/backend.o build/platform.o $(OBJECTS)
	$(CC) -shared -Wl,--gc-sections -o $@ $^ -lSDL -ldl -lm
-include build/backend.d build/platform.d $(OBJECTS:.o=.d)
test-touch: test-touch.c touch_events.h
	$(CC) -Os -Wall -Wextra -o $@ test-touch.c
