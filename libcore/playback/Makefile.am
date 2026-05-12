
include $(top_srcdir)/Makefile.inc

lib_LTLIBRARIES = libcdx_playback.la

if TEST_ONLY_ENABLE_AUDIO
libcdx_playback_la_SOURCES = audioDecComponent.c     \
    audioRenderComponent.c              \
    avtimer.c                           \
    bitrateEstimater.c                  \
    framerateEstimater.c                \
    streamManager.c                     \
    player.c                            \
    baseComponent.c
else
libcdx_playback_la_SOURCES = videoDecComponent.c                 \
    avtimer.c                           \
    bitrateEstimater.c                  \
    framerateEstimater.c                \
    streamManager.c                     \
    player.c                            \
    videoRenderComponent_newDisplay.c   \
    baseComponent.c
if !ONLY_DISABLE_AUDIO
libcdx_playback_la_SOURCES += audioDecComponent.c     \
    audioRenderComponent.c
endif
if CONFIG_ENABLE_SUBTITLE
libcdx_playback_la_SOURCES += subtitleDecComponent.c	\
    subtitleRenderComponent.c
endif
endif

#	videoRenderComponent_newDisplay.c

libcdx_playback_la_CFLAGS = $(CFLAGS_CDXG)

#libcdx_playback_la_CPPFLAGS = $(CFLAGS_CPPDXG)

if TEST_ONLY_ENABLE_AUDIO
LOCAL_INCLUDE = -I$(top_srcdir)/libcore/include \
		-I$(top_srcdir)/libcore/base/include \
		-I$(top_srcdir)/libcore/common/iniparser \
		-I$(top_srcdir)/ \
		-I$(top_srcdir)/external/include/adecoder/ \
		-I./include
else
LOCAL_INCLUDE = -I$(top_srcdir)/libcore/include \
		-I$(top_srcdir)/libcore/base/include \
		-I$(top_srcdir)/libcore/common/iniparser \
		-I$(top_srcdir)/ \
		-I$(top_srcdir)/../mpp_libcedarc/include \
		-I$(top_srcdir)/../libcedarc/include \
		-I./include
if !ONLY_DISABLE_AUDIO
LOCAL_INCLUDE += -I$(top_srcdir)/external/include/adecoder/
endif
endif

libcdx_playback_la_CFLAGS += $(LOCAL_INCLUDE)

#libcdx_playback_la_CPPFLAGS += $(LOCAL_INCLUDE)


libcdx_playback_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_playback_la_LIBADD = $(top_srcdir)/libcore/common/libcdx_common.la \
			$(top_srcdir)/libcore/base/libcdx_base.la

if TEST_ONLY_ENABLE_AUDIO
LOCAL_LIB = -lpthread -ldl -lrt
else
LOCAL_LIB = -lpthread -ldl -lrt -lvdecoder -lMemAdapter
endif
if CONFIG_ENABLE_SUBTITLE
LOCAL_LIB += -lsubdecoder
endif
libcdx_playback_la_LDFLAGS += $(LOCAL_LIB)
