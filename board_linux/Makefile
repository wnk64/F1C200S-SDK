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

# Keep the protected Miracast player and its object files untouched.
PLAYER_SRC := $(abspath ../../../SOFTWARE/cedar-player/player-src)
PLAYER_C := main.c driver/drm_warpper.c render/mediaplayer.c utils/cedar_cma_pool.c utils/spsc_queue.c utils/log.c utils/misc.c
PLAYER_OBJ := $(addprefix build-arm/net-player/,$(PLAYER_C:.c=.o))
SYSROOT := $(shell $(CC) --print-sysroot)
net-player: build-arm/cedar-net-player
build-arm/net-player/render/mediaplayer.o: loop_mediaplayer.c $(PLAYER_SRC)/render/mediaplayer.c
	@mkdir -p $(@D)
	$(CC) -Os -std=gnu11 -Wall -Wextra -Wno-unused-parameter -D_GNU_SOURCE -DHAVE_CEDARX=1 -I$(PLAYER_SRC) -I$(PLAYER_SRC)/render -I$(SYSROOT)/usr/include/drm -I../../../ROOTFS/buildroot-2018.02.11/output/build/libcedarx-custom -MMD -MP -c -o $@ $<
build-arm/net-player/%.o: $(PLAYER_SRC)/%.c
	@mkdir -p $(@D)
	$(CC) -Os -std=gnu11 -Wall -Wextra -Wno-unused-parameter -D_GNU_SOURCE -DHAVE_CEDARX=1 -I$(PLAYER_SRC) -I$(SYSROOT)/usr/include/drm -I../../../ROOTFS/buildroot-2018.02.11/output/build/libcedarx-custom -MMD -MP -c -o $@ $<
build-arm/cdx_https.o: cdx_https.c
	$(CC) -Os -std=gnu11 -Wall -Wextra -D_GNU_SOURCE -Ivendor/curl-8.16.0/include -c -o $@ $<
build-arm/cedar_osd.o: cedar_osd.c
	$(CC) -Os -std=gnu11 -Wall -Wextra -D_GNU_SOURCE -I$(PLAYER_SRC) -I$(SYSROOT)/usr/include/drm -c -o $@ $<
build-arm/cedar-net-player: $(PLAYER_OBJ) build-arm/cdx_https.o build-arm/cedar_osd.o build-curl/lib/.libs/libcurl.a
	$(CC) -Wl,--gc-sections,--wrap=drm_warpper_init,--wrap=drm_warpper_destroy -L../../../ROOTFS/buildroot-2018.02.11/output/target/usr/lib -o $@ $^ -ldrm -lMemAdapter -lvdecoder -lVE -lvideoengine -lcdc_base -lcdx_parser -lcdx_stream -lcdx_base -lcdx_common -lfdk-aac -lasound -lssl -lcrypto -lz -lpthread -lrt -ldl -lm
-include $(PLAYER_OBJ:.o=.d)
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
