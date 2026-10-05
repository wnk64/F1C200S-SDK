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
COMMAND_FILES := ScriptParser ScriptParser_command DirectReader SarReader NsaReader FontInfo AnimationInfo DirtyRect resize_image
COMMAND_OBJECTS := $(addprefix build/engine/,$(addsuffix .o,$(COMMAND_FILES))) build/charset_bridge.o build/animation_alpha.o build/font_info_bridge.o build/ScriptHandler.o
EXECUTION_FILES := ONScripter ONScripter_animation ONScripter_command ONScripter_effect ONScripter_effect_breakup ONScripter_event ONScripter_file ONScripter_file2 ONScripter_image ONScripter_lut ONScripter_rmenu ONScripter_sound ONScripter_text
EXECUTION_OBJECTS := $(addprefix build/engine/,$(addsuffix .o,$(EXECUTION_FILES)))
all: libons-native.a libons-script.a libons-commands.a native-tests native-event-tests native-sprite-tests native-audio-tests native-animation-tests native-font-tests native-runtime-image-tests native-runtime-event-tests ons-native-probe ons-script-probe ons-audio-probe ons-command-probe
.PHONY: verify-script-source
verify-script-source:
	sha256sum -c source-baseline.sha256
build/engine/generated.stamp: prepare_engine.py source-baseline.sha256 | verify-script-source
	python3 prepare_engine.py build/engine
$(COMMAND_OBJECTS): build/engine/generated.stamp
$(addprefix build/engine/,$(addsuffix .o,$(COMMAND_FILES) $(EXECUTION_FILES))): build/engine/%.o: build/engine/generated.stamp engine_types.h engine_runtime.h
	$(CXX) -O2 -Wall -funsigned-char -fno-strict-aliasing -ffunction-sections -fdata-sections -MMD -MP -DCHARSET_GBK -DLINUX -DPDA_AUTOSIZE -D_FILE_OFFSET_BITS=64 -I. -Ibuild/engine -I$(ENGINE) -I$(EXT)/include -c -o $@ build/engine/$*.cpp
build/charset_bridge.o: charset_bridge.cpp native.h
	@mkdir -p $(@D)
	$(CXX) -O2 -Wall -MMD -MP -c -o $@ $<
build/animation_alpha.o: animation_alpha.cpp build/engine/generated.stamp engine_types.h
	$(CXX) -O2 -Wall -MMD -MP -I. -Ibuild/engine -c -o $@ $<
build/command_probe.o: command_probe.cpp build/engine/generated.stamp engine_types.h
	$(CXX) -O2 -Wall -funsigned-char -MMD -MP -DCHARSET_GBK -DLINUX -D_FILE_OFFSET_BITS=64 -I. -Ibuild/engine -I$(ENGINE) -c -o $@ $<
libons-commands.a: $(COMMAND_OBJECTS)
	$(AR) rcs $@ $^
.PHONY: execution-objects
execution-objects: $(EXECUTION_OBJECTS)
build/native_main.o: native_main.cpp build/engine/generated.stamp engine_runtime.h
	$(CXX) -O2 -Wall -funsigned-char -MMD -MP -DCHARSET_GBK -DLINUX -I. -Ibuild/engine -c -o $@ $<
# Link intentionally fails until all native runtime contracts have implementations.
ons-lvgl: build/native_main.o $(EXECUTION_OBJECTS) build/runtime_image.o build/runtime_display.o build/platform.o $(OBJECTS) libons-commands.a libons-native.a
	$(CXX) -Wl,--gc-sections -o $@ $^ $(EXT)/lib/libvorbisfile.a $(EXT)/lib/libvorbis.a $(EXT)/lib/libogg.a $(EXT)/lib/libbz2.a $(LIBS) -lasound
build/runtime_image.o: runtime_image.cpp engine_runtime.h
	$(CXX) -O2 -Wall -ffunction-sections -fdata-sections -MMD -MP -c -o $@ $<
build/runtime_image_tests.o: runtime_image_tests.cpp engine_runtime.h
	$(CXX) -O2 -Wall -MMD -MP -c -o $@ $<
native-runtime-image-tests: build/runtime_image_tests.o build/runtime_image.o libons-native.a
	$(CXX) -Wl,--gc-sections -o $@ $^ $(LIBS)
build/runtime_display.o: runtime_display.cpp engine_runtime.h
	$(CXX) $(CFLAGS) -c -o $@ $<
build/runtime_event_tests.o: runtime_event_tests.cpp engine_runtime.h
	$(CXX) -O2 -Wall -MMD -MP -c -o $@ $<
native-runtime-event-tests: build/runtime_event_tests.o build/runtime_display.o build/platform.o $(OBJECTS) libons-native.a
	$(CXX) -Wl,--gc-sections -o $@ $^ $(LIBS)
build/animation_tests.o: animation_tests.cpp build/engine/generated.stamp engine_types.h
	$(CXX) -O2 -Wall -MMD -MP -I. -Ibuild/engine -c -o $@ $<
native-animation-tests: build/animation_tests.o libons-commands.a build/native.o build/surface.o build/sprite.o build/gbk.o
	$(CXX) -Wl,--gc-sections -o $@ $^ $(LIBS)
build/font_info_bridge.o: font_info_bridge.cpp font.h build/engine/generated.stamp
	$(CXX) -O2 -Wall -MMD -MP -I. -Ibuild/engine -c -o $@ $<
build/font_tests.o: font_tests.cpp font.h build/engine/generated.stamp
	$(CXX) -O2 -Wall -MMD -MP -I. -Ibuild/engine -c -o $@ $<
native-font-tests: build/font_tests.o libons-commands.a build/native.o build/font.o build/surface.o build/sprite.o build/gbk.o
	$(CXX) -Wl,--gc-sections -o $@ $^ $(LIBS)
ons-command-probe: build/command_probe.o libons-commands.a build/native.o build/surface.o build/sprite.o build/gbk.o
	$(CXX) -Wl,--gc-sections -o $@ $^ $(EXT)/lib/libbz2.a $(LIBS)
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
libons-native.a: build/native.o build/font.o build/surface.o build/archive.o build/gbk.o build/events.o build/sprite.o build/audio.o
	$(AR) rcs $@ $^
native-tests: build/tests.o build/native.o build/surface.o build/archive.o build/gbk.o
	$(CC) -Wl,--gc-sections -o $@ $^ $(LIBS)
build/events.o: events.h
build/event_tests.o: events.h
native-event-tests: build/event_tests.o build/events.o
	$(CC) -Wl,--gc-sections -o $@ $^ -lpthread
build/sprite.o: sprite.h
build/sprite_tests.o: sprite.h
native-sprite-tests: build/sprite_tests.o build/sprite.o build/native.o build/surface.o build/gbk.o
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
ons-native-probe: build/probe.o build/native.o build/surface.o build/archive.o build/gbk.o build/sprite.o build/audio.o build/events.o build/platform.o $(OBJECTS)
	$(CC) -Wl,--gc-sections -o $@ $^ $(LIBS) $(EXT)/lib/libvorbisfile.a $(EXT)/lib/libvorbis.a $(EXT)/lib/libogg.a -lasound
-include $(wildcard build/*.d) $(wildcard build/engine/*.d) $(OBJECTS:.o=.d)
