
include $(top_srcdir)/Makefile.inc

noinst_LTLIBRARIES = libcdx_rtmp_stream.la

## set the source files.
libcdx_rtmp_stream_la_SOURCES =  CdxRtmpStream.c \
								 CdxRtmpConnect.c

libcdx_rtmp_stream_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I../../include \
		-I../../base/include \
		-I$(top_srcdir)/


libcdx_rtmp_stream_la_CFLAGS += $(LOCAL_INCLUDE)

libcdx_rtmp_stream_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_rtmp_stream_la_LIBADD = $(top_srcdir)/libcore/base/libcdx_base.la

libcdx_rtmp_stream_la_LDFLAGS += $(LOCAL_LIB)
