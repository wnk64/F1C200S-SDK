#!/bin/sh
set -eu
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
ROOT=$(CDPATH= cd "$SELF/../.." && pwd)
HOST="$ROOT/ROOTFS/buildroot-2018.02.11/output/host"
export PKG_CONFIG_SYSROOT_DIR="$HOST/arm-buildroot-linux-gnueabi/sysroot"
export PKG_CONFIG_LIBDIR="$PKG_CONFIG_SYSROOT_DIR/usr/lib/pkgconfig:$PKG_CONFIG_SYSROOT_DIR/usr/share/pkgconfig"
export PATH="$HOST/bin:$PATH"
exec make -C "$SELF/vendor/mupdf-1.24.9-source" -j"${JOBS:-2}" \
    build=small OUT=build/f1-arm-small OS=Linux \
    CC="$HOST/bin/arm-buildroot-linux-gnueabi-gcc" \
    CXX="$HOST/bin/arm-buildroot-linux-gnueabi-g++" \
    LD="$HOST/bin/arm-buildroot-linux-gnueabi-ld" \
    AR="$HOST/bin/arm-buildroot-linux-gnueabi-ar" \
    RANLIB="$HOST/bin/arm-buildroot-linux-gnueabi-ranlib" \
    USE_SYSTEM_LIBJPEG=yes USE_SYSTEM_ZLIB=yes USE_SYSTEM_FREETYPE=no \
    HAVE_GLUT=no HAVE_X11=no HAVE_LIBCRYPTO=no HAVE_JPEGXR=no \
    USE_TESSERACT=no HAVE_TESSERACT=no HAVE_LEPTONICA=no \
    XCFLAGS="-DTOFU_NOTO -DTOFU_CJK_EXT -DFZ_ENABLE_ICC=0 -DFZ_ENABLE_JS=0 -DFZ_ENABLE_XPS=0 -DFZ_ENABLE_OCR_OUTPUT=0 -DFZ_ENABLE_DOCX_OUTPUT=0 -DFZ_ENABLE_ODT_OUTPUT=0" libs
