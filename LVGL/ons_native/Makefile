ROOT := /home/wnk/Desktop/F1c200S_SOFT
HOST := $(ROOT)/ROOTFS/buildroot-2018.02.11/output/host
CC := $(HOST)/bin/arm-buildroot-linux-gnueabi-gcc
CXX := $(HOST)/bin/arm-buildroot-linux-gnueabi-g++
AR := $(HOST)/bin/arm-buildroot-linux-gnueabi-ar
MENU := $(ROOT)/LVGL/menu
LV := $(ROOT)/LVGL/touch/lvgl
ENGINE := $(ROOT)/SOFTWARE/legacy-LicheePi_Nano/third_party/ONScripter-GBK
EXT := $(ROOT)/SOFTWARE/legacy-LicheePi_Nano/third_party/ONScripter-EN/extlib
SOURCES := $(wildcard $(LV)/src/*/*.c) $(wildcard $(LV)/src/*/*/*.c) $(wildcard $(LV)/src/*/*/*/*.c)
OBJECTS := $(patsubst $(LV)/%.c,build/%.o,$(SOURCES))
CFLAGS := -Os -Wall -Wextra -ffunction-sections -fdata-sections -MMD -MP -D_FILE_OFFSET_BITS=64
CFLAGS += -DLV_CONF_INCLUDE_SIMPLE -I$(MENU) -I$(ROOT)/LVGL/touch -I$(LV)
CFLAGS += -I$(HOST)/arm-buildroot-linux-gnueabi/sysroot/usr/include/freetype2
LIBS := -lpng -ljpeg -lfreetype -lm -lpthread
all: libons-native.a libons-script.a native-tests native-event-tests native-sprite-tests native-audio-tests ons-native-probe ons-script-probe ons-audio-probe
.PHONY: verify-script-source
verify-script-source:
	sha256sum -c source-baseline.sha256
build/ScriptHandler.o: $(ENGINE)/ScriptHandler.cpp $(ENGINE)/ScriptHandler.h $(ENGINE)/BaseReader.h | verify-script-source
	@mkdir -p $(@D)
	$(CXX) -O2 -Wall -funsigned-char -fno-strict-aliasing -ffunction-sections -fdata-sections -MMD -MP -DCHARSET_GBK -DLINUX -D_FILE_OFFSET_BITS=64 -I$(ENGINE) -c -o $@ $<
build/script_probe.o: script_probe.cpp $(ENGINE)/ScriptHandler.h | verify-script-source
	@mkdir -p $(@D)
	$(CXX) -O2 -Wall -funsigned-char -MMD -MP -DCHARSET_GBK -DLINUX -D_FILE_OFFSET_BITS=64 -I$(ENGINE) -c -o $@ $<
libons-script.a: build/ScriptHandler.o
	$(AR) rcs $@ $^
ons-script-probe: build/script_probe.o libons-script.a
	$(CXX) -Wl,--gc-sections -o $@ $^
build/gbk_map.h: generate_gbk.py
	@mkdir -p $(@D)
	python3 generate_gbk.py $@
build/gbk.o: build/gbk_map.h
build/%.o: $(LV)/%.c $(MENU)/lv_conf.h
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c -o $@ $<
build/%.o: %.c native.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<
build/platform.o: $(MENU)/platform.c $(MENU)/platform.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<
libons-native.a: build/native.o build/archive.o build/gbk.o build/events.o build/sprite.o build/audio.o
	$(AR) rcs $@ $^
native-tests: build/tests.o build/native.o build/archive.o build/gbk.o
	$(CC) -Wl,--gc-sections -o $@ $^ $(LIBS)
build/events.o: events.h
build/event_tests.o: events.h
native-event-tests: build/event_tests.o build/events.o
	$(CC) -Wl,--gc-sections -o $@ $^ -lpthread
build/sprite.o: sprite.h
build/sprite_tests.o: sprite.h
native-sprite-tests: build/sprite_tests.o build/sprite.o build/native.o build/gbk.o
	$(CC) -Wl,--gc-sections -o $@ $^ $(LIBS)
build/audio.o: audio.c audio.h events.h native.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I$(EXT)/include -c -o $@ $<
build/audio_probe.o: audio.h events.h
build/audio_tests.o: audio.h events.h
native-audio-tests: build/audio_tests.o build/audio.o build/archive.o build/events.o
	$(CC) -Wl,--gc-sections -o $@ $^ $(EXT)/lib/libvorbisfile.a $(EXT)/lib/libvorbis.a $(EXT)/lib/libogg.a -lasound -lm -lpthread
ons-audio-probe: build/audio_probe.o build/audio.o build/archive.o build/events.o
	$(CC) -Wl,--gc-sections -o $@ $^ $(EXT)/lib/libvorbisfile.a $(EXT)/lib/libvorbis.a $(EXT)/lib/libogg.a -lasound -lm -lpthread
ons-native-probe: build/probe.o build/native.o build/archive.o build/gbk.o build/sprite.o build/audio.o build/events.o build/platform.o $(OBJECTS)
	$(CC) -Wl,--gc-sections -o $@ $^ $(LIBS) $(EXT)/lib/libvorbisfile.a $(EXT)/lib/libvorbis.a $(EXT)/lib/libogg.a -lasound
-include $(wildcard build/*.d) $(OBJECTS:.o=.d)
