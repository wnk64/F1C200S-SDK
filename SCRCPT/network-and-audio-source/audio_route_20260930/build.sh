#!/bin/bash
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
HOST=/home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/host
SYSROOT="$HOST/arm-buildroot-linux-gnueabi/sysroot"
PKG_CONFIG_SYSROOT_DIR="$SYSROOT" \
PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/pkgconfig" \
"$HOST/bin/pkg-config" --cflags --libs dbus-1 > /tmp/f1-audio-dbus-flags
mkdir -p "$HERE/artifacts"
"$HOST/bin/arm-buildroot-linux-gnueabi-gcc" -std=gnu99 -Wall -Wextra -O2 \
    -I "$HERE" "$HERE/audio-route.c" "$HERE/shared/dbus-client.c" \
    $(cat /tmp/f1-audio-dbus-flags) -lbluetooth -o "$HERE/artifacts/f1-audio-route"
rm /tmp/f1-audio-dbus-flags
md5sum "$HERE/artifacts/f1-audio-route"
