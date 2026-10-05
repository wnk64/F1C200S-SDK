#!/bin/sh
set -eu
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
ROOT=$(CDPATH= cd "$SELF/../.." && pwd)
HOST="$ROOT/ROOTFS/buildroot-2018.02.11/output/host"
SYSROOT="$HOST/arm-buildroot-linux-gnueabi/sysroot"
TARGET="$ROOT/ROOTFS/buildroot-2018.02.11/output/target"
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
export PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig"
export PATH="$HOST/bin:$PATH"
cd "$SELF"
./configure --prefix=/usr --confdir=/etc \
    --target=arm-buildroot-linux-gnueabi --host-cc=/usr/bin/gcc \
    --cc="$HOST/bin/arm-buildroot-linux-gnueabi-gcc" \
    --as="$HOST/bin/arm-buildroot-linux-gnueabi-as" \
    --extra-cflags="-Os -D_FILE_OFFSET_BITS=64 -I$SYSROOT/usr/include -I$SYSROOT/usr/include/drm" \
    --extra-ldflags="-L$SYSROOT/usr/lib -L$TARGET/usr/lib -Wl,-rpath-link,$TARGET/usr/lib" \
    --enable-cross-compile --enable-armv5te --disable-runtime-cpudetection \
    --charset=UTF-8 --disable-big-endian --enable-fbdev --enable-alsa \
    --disable-sdl --enable-freetype --with-freetype-config="$SELF/freetype-config-current.sh" \
    --disable-fontconfig --disable-enca --disable-fribidi --disable-iconv \
    --disable-smb --disable-bluray --disable-libcdio --enable-mplayer --disable-mencoder \
    --enable-faad --disable-mp3lame --enable-mad --disable-live --enable-gif \
    --disable-librtmp --disable-speex --disable-liblzo --disable-ivtv \
    --disable-mmx --disable-mmxext --disable-sse --disable-sse2 --disable-sse3 \
    --disable-ssse3 --disable-sse4 --disable-sse42 --disable-avx --disable-avx2 \
    --enable-dynamic-plugins --enable-inet6
make -j"${JOBS:-2}" CEDAR_TARGET_LIBDIR="$TARGET/usr/lib" mplayer
"$HOST/bin/arm-buildroot-linux-gnueabi-readelf" -d mplayer
if "$HOST/bin/arm-buildroot-linux-gnueabi-readelf" -d mplayer | grep -qi SDL; then
    echo 'Unexpected SDL dependency in candidate' >&2
    exit 1
fi
