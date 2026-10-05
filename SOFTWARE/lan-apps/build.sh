#!/bin/bash
set -euo pipefail
ROOT=/home/wnk/Desktop/F1c200S_SOFT
SELF="$ROOT/SOFTWARE/lan-apps"
HOST="$ROOT/ROOTFS/buildroot-2018.02.11/output/host"
CC="$HOST/bin/arm-buildroot-linux-gnueabi-gcc"
SYSROOT=$($CC --print-sysroot)
cd "$SELF"
mkdir -p build stage
find vendor -type f \( -name '*.am' -o -name '*.ac' -o -name '*.m4' -o -name '*.sh' -o -name '*.c' -o -name '*.h' -o -name '*.in' \) -exec sed -i 's/\r$//' {} +
cd vendor/pupnp
autoreconf -fi
./configure --host=arm-buildroot-linux-gnueabi --prefix="$SELF/stage" CC="$CC" CFLAGS='-Os' --disable-samples --disable-shared --enable-static --disable-ipv6
make -j2
make install
cd "$SELF/vendor/gmrender"
autoreconf -fi
PKG_CONFIG_LIBDIR="$SELF/stage/lib/pkgconfig:$SYSROOT/usr/lib/pkgconfig" PKG_CONFIG_SYSROOT_DIR="$SYSROOT" \
    ./configure --host=arm-buildroot-linux-gnueabi --prefix="$SELF/stage" CC="$CC" CFLAGS='-Os' --without-gstreamer \
    GLIB_CFLAGS="-I$SYSROOT/usr/include/glib-2.0 -I$SYSROOT/usr/lib/glib-2.0/include" GLIB_LIBS='-lgthread-2.0 -lglib-2.0' \
    LIBUPNP_CFLAGS="-I$SELF/stage/include/upnp" LIBUPNP_LIBS="$SELF/stage/lib/libupnp.a $SELF/stage/lib/libixml.a -lpthread"
make -j2
cp src/gmediarender "$SELF/"
cd "$SELF"
$CC -Os -std=gnu99 -D_GNU_SOURCE -DNO_SSL -DNO_CGI -DNO_WEBSOCKET -DUSE_STACK_SIZE=131072 -Ivendor/civetweb/include -c vendor/civetweb/src/civetweb.c -o build/civetweb.o
# Reuse existing LVGL8/menu platform/font compiled with the same target toolchain.
make -C "$ROOT/LVGL/menu" f1-menu
$CC -Os -std=gnu99 -Wall -Wextra -DLV_CONF_INCLUDE_SIMPLE -Ivendor/civetweb/include -I"$ROOT/LVGL/menu" -I"$ROOT/LVGL/touch" -I"$ROOT/LVGL/touch/lvgl" -c lan_ui.c -o build/lan_ui.o
$CC -o lan-ui build/lan_ui.o build/civetweb.o "$ROOT/LVGL/menu/platform.o" "$ROOT/LVGL/menu/menu_font_stream.o" $(find "$ROOT/LVGL/menu/build/lvgl" -name '*.o') -lm -lpthread -ldl
sha256sum lan-ui gmediarender web-app.sh dlna-app.sh
