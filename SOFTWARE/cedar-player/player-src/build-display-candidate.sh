#!/bin/sh
set -eu
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
HOST=/home/wnk/Desktop/F1c200S_SOFT/ROOTFS/buildroot-2018.02.11/output/host
CC=$HOST/bin/arm-buildroot-linux-gnueabi-gcc
SYSROOT=$($CC --print-sysroot)
TARGET=/home/wnk/Desktop/F1c200S_SOFT/ROOTFS/buildroot-2018.02.11/output/target
CEDARX=/home/wnk/Desktop/F1c200S_SOFT/ROOTFS/buildroot-2018.02.11/output/build/libcedarx-custom
OUT=$SELF/build-display-bounded
mkdir -p "$OUT"
for source in main.c driver/drm_warpper.c render/mediaplayer.c utils/cedar_cma_pool.c utils/spsc_queue.c utils/log.c utils/misc.c; do
    object=$(printf '%s' "$source" | tr / _).o
    "$CC" --sysroot="$SYSROOT" -Os -Wall -Wextra -Wno-unused-parameter -D_GNU_SOURCE -DHAVE_CEDARX=1 \
        -I"$SELF" -I"$SYSROOT/usr/include" -I"$SYSROOT/usr/include/drm" -I"$CEDARX" \
        -c "$SELF/$source" -o "$OUT/$object"
done
"$CC" --sysroot="$SYSROOT" -L"$SYSROOT/usr/lib" -L"$TARGET/usr/lib" -o "$OUT/player-display-bounded" "$OUT"/*.o \
    -ldrm -lMemAdapter -lvdecoder -lVE -lvideoengine -lcdc_base -lcdx_parser -lcdx_stream -lcdx_base -lcdx_common \
    -lfdk-aac -lasound -lssl -lcrypto -lz -lpthread -lrt -ldl -lm
"$HOST/bin/arm-buildroot-linux-gnueabi-strip" "$OUT/player-display-bounded"
sha256sum "$OUT/player-display-bounded"
