ROOT := /home/wnk/Desktop/F1c200S_SOFT
HOST := $(ROOT)/ROOTFS/buildroot-2018.02.11/output/host
CC := $(HOST)/bin/arm-buildroot-linux-gnueabi-gcc
LV := $(ROOT)/LVGL/touch/lvgl
MUPDF := vendor/mupdf-1.24.9-source
DOC_LIBS := $(MUPDF)/build/f1-arm-small/libmupdf.a $(MUPDF)/build/f1-arm-small/libmupdf-third.a
CFLAGS := -Os -std=gnu99 -Wall -Wextra -D_GNU_SOURCE -DLV_CONF_INCLUDE_SIMPLE -I. -I$(ROOT)/LVGL/menu -I$(ROOT)/LVGL/touch -I$(LV) -I$(MUPDF)/include -ffunction-sections -fdata-sections -MMD -MP
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
$(DOC_LIBS): build-document-lib.sh
	sh build-document-lib.sh
reader: build/reader.o build/image.o build/text.o build/document.o build/reader_font_28.o build/platform.o build/font.o $(OBJECTS) $(DOC_LIBS)
	$(CC) -Wl,--gc-sections -o $@ $(filter-out $(DOC_LIBS),$^) -Wl,--start-group $(DOC_LIBS) -Wl,--end-group -ljpeg -lz -lstdc++ -lm -lpthread
reader-browser: build/reader_browser.o build/platform.o build/font.o $(OBJECTS)
	$(CC) -Wl,--gc-sections -o $@ $^ -lm
-include build/*.d $(OBJECTS:.o=.d)
