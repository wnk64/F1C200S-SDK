SDK ?= $(abspath ../../../ROOTFS/buildroot-2018.02.11/output/host)
CC := $(SDK)/bin/arm-buildroot-linux-gnueabi-gcc
CXX := $(SDK)/bin/arm-buildroot-linux-gnueabi-g++
STRIP := $(SDK)/bin/arm-buildroot-linux-gnueabi-strip
LV := ../third_party/lvgl
CPPFLAGS := -DWILIWILI_BOARD -DLV_CONF_INCLUDE_SIMPLE -I.. -I../third_party -I$(LV) -I$(LV)/src -I../windows_lvgl/src -I../../menu -Ivendor/curl-8.16.0/include
CFLAGS := -Os -std=gnu99 -ffunction-sections -fdata-sections -MMD -MP
CXXFLAGS := -Os -std=c++1z -ffunction-sections -fdata-sections -MMD -MP -pthread
LVOBJ := $(patsubst $(LV)/%.c,build-arm/lvgl/%.o,$(shell find $(LV)/src -name '*.c'))
OBJ := build-arm/main.o build-arm/platform_linux.o build-arm/jpeg_thumbnail.o build-arm/board_font.o $(LVOBJ)
.DEFAULT_GOAL := all
all: build-arm/wiliwili-lvgl build-arm/platform-probe
build-arm/wiliwili-lvgl: $(OBJ) build-curl/lib/.libs/libcurl.a
	$(CXX) -Wl,--gc-sections -o $@ $^ -lssl -lcrypto -ljpeg -pthread -lm
build-arm/main.o: ../windows_lvgl/src/main.cpp compat_cpp17.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<
build-arm/platform-probe: build-arm/platform_probe.o build-arm/platform_linux.o build-arm/jpeg_thumbnail.o build-arm/board_font.o $(LVOBJ) build-curl/lib/.libs/libcurl.a
	$(CXX) -Wl,--gc-sections -o $@ $^ -lssl -lcrypto -ljpeg -pthread -lm
build-arm/platform_probe.o: platform_probe.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<
build-arm/platform_linux.o: platform_linux.cpp geometry.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<
build-arm/jpeg_thumbnail.o: jpeg_thumbnail.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
build-arm/board_font.o: board_font.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
build-arm/lvgl/%.o: $(LV)/%.c ../lv_conf.h
	@mkdir -p $(@D)
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
strip: all
	$(STRIP) build-arm/wiliwili-lvgl
-include $(OBJ:.o=.d)
