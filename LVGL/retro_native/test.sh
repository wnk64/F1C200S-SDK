#!/bin/sh
set -eu
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
cd "$SELF"
HOST=/home/wnk/Desktop/F1c200S_SOFT/ROOTFS/buildroot-2018.02.11/output/host
SYSROOT=$HOST/arm-buildroot-linux-gnueabi/sysroot
make -s -j2
mkdir -p build/bios build/saves
export RETRO_SYSTEM_DIR="$SELF/build/bios" RETRO_SAVE_DIR="$SELF/build/saves"
gcc -std=gnu99 -O1 -g -fsanitize=address,undefined test_pixels.c -o build/test-pixels
ASAN_OPTIONS=detect_leaks=1 build/test-pixels
gcc -std=gnu99 make_md_fixture.c -o build/make-md
build/make-md build/native.md
gcc -std=gnu99 make_ps1_fixture.c -o build/make-ps1
build/make-ps1 build/native.exe
"$HOST/bin/arm-buildroot-linux-gnueabi-as" -march=armv4t gba_fixture.s -o build/gba_fixture.o
"$HOST/bin/arm-buildroot-linux-gnueabi-objcopy" -O binary build/gba_fixture.o build/native.gba
for system in gba ps1 md;do
    case $system in gba)rom=native.gba;frames=60;;ps1)rom=native.exe;frames=10;;md)rom=native.md;frames=60;;esac
    timeout 30 qemu-arm-static -L "$SYSROOT" ./retro-native --probe "build/cores/$system.so" >"build/probe-$system.log" 2>&1
    timeout 40 qemu-arm-static -L "$SYSROOT" ./retro-native --headless "$frames" "$system" "build/cores/$system.so" "build/$rom" >"build/test-$system.stdout" 2>"build/test-$system.log"
    tail -1 "build/test-$system.log"
    grep -Eq "native runs=$frames video=[1-9][0-9]* changes=[1-9][0-9]* nonblack=[1-9][0-9]*" "build/test-$system.log"
    qemu-arm-static -L "$SYSROOT" ./retro-browser "$system" "build/$rom" --check
done
for script in retro-app.sh emulators-app.sh gba-app.sh ps1-app.sh md-app.sh;do sh -n "$script";done
echo 'Three cores: ARM init/execution/changing-pixels/browser plus pixel-sanitizer PASS (not physical performance)'
