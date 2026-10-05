#!/bin/sh
set -eu
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
ROOT=$(CDPATH= cd "$SELF/../.." && pwd)
HOST="$ROOT/ROOTFS/buildroot-2018.02.11/output/host"
export PKG_CONFIG_SYSROOT_DIR="$HOST/arm-buildroot-linux-gnueabi/sysroot"
export PKG_CONFIG_LIBDIR="$PKG_CONFIG_SYSROOT_DIR/usr/lib/pkgconfig"
case "${1:-}" in
    --cflags) exec "$HOST/bin/pkg-config" --cflags freetype2 ;;
    --libs) exec "$HOST/bin/pkg-config" --libs freetype2 ;;
    --ftversion|--version) exec "$HOST/bin/pkg-config" --modversion freetype2 ;;
    *) exit 1 ;;
esac
