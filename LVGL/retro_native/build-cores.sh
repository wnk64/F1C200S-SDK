#!/bin/sh
set -eu
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
HOST=/home/wnk/Desktop/F1c200S_SOFT/ROOTFS/buildroot-2018.02.11/output/host
CC=$HOST/bin/arm-buildroot-linux-gnueabi-gcc
CXX=$HOST/bin/arm-buildroot-linux-gnueabi-g++
AR=$HOST/bin/arm-buildroot-linux-gnueabi-ar
mkdir -p "$SELF/build/cores"
export SOURCE_DATE_EPOCH=1791590400
make -B -C "$SELF/vendor/gpsp" -j2 platform=miyoo CC="$CC" CXX="$CXX" AR="$AR" GIT_VERSION=5819380c SHARED='-shared -Wl,--version-script=link.T' MMAP_JIT_CACHE=1 >"$SELF/build/gpsp.log" 2>&1
cp "$SELF/vendor/gpsp/gpsp_libretro.so" "$SELF/build/cores/gba.so"
make -C "$SELF/vendor/pcsx" -f Makefile.libretro -W frontend/libretro.c -W frontend/main.c -j2 platform=miyoo CC="$CC" CXX="$CXX" AR="$AR" GIT_VERSION=acfca1d WANT_ZLIB=0 HAVE_CHD=0 HAVE_PHYSICAL_CDROM=0 USE_ASYNC_CDROM=0 USE_ASYNC_GPU=0 USE_ASYNC_SPU=0 NDRC_THREAD=0 >"$SELF/build/pcsx.log" 2>&1
cp "$SELF/vendor/pcsx/pcsx_rearmed_libretro.so" "$SELF/build/cores/ps1.so"
g++ -O2 -DHAVE_ARMv6=0 -DCONFIG_FILE='"../cyclone_config.h"' "$SELF"/vendor/pico/cpu/cyclone/*.cpp "$SELF"/vendor/pico/cpu/cyclone/Disa/Disa.c -o "$SELF/build/cyclone-gen" >"$SELF/build/cyclone.log" 2>&1
(cd "$SELF/vendor/pico/cpu/cyclone" && "$SELF/build/cyclone-gen") >>"$SELF/build/cyclone.log" 2>&1
make -C "$SELF/vendor/pico" -f Makefile.libretro -W platform/libretro/libretro.c -j2 platform=miyoo CC="$CC" CXX="$CXX" AR="$AR" GIT_REVISION=-26ecb2b SHARED=-shared LIBM=-lm use_libchdr=0 >"$SELF/build/pico.log" 2>&1
cp "$SELF/vendor/pico/picodrive_libretro.so" "$SELF/build/cores/md.so"
sha256sum "$SELF/build/cores/"*.so
