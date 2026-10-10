#!/bin/sh
set -eu
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
cd "$SELF"
mkdir -p build
gcc -std=gnu99 -O1 -g -fsanitize=address,undefined test_blit.c -o build/test-blit
ASAN_OPTIONS=detect_leaks=1 build/test-blit
gcc -std=gnu99 -Wall -Wextra make_fixture.c -o build/make-fixture
build/make-fixture build/native-fixture.sfc
make -s -j2
SYSROOT=/home/wnk/Desktop/F1c200S_SOFT/ROOTFS/buildroot-2018.02.11/output/host/arm-buildroot-linux-gnueabi/sysroot
timeout 40 qemu-arm-static -L "$SYSROOT" ./sfc-native --headless 120 build/native-fixture.sfc >build/core.stdout 2>build/core.log
cat build/core.log
grep -Eq 'frames=120 draws=120 changes=[1-9][0-9]* nonblack=[1-9][0-9]*' build/core.log
qemu-arm-static -L "$SYSROOT" ./sfc-browser build/native-fixture.sfc --check
qemu-arm-static -L "$SYSROOT" ./sfc-browser build --list
if qemu-arm-static -L "$SYSROOT" ./sfc-native --headless 0 build/native-fixture.sfc; then exit 1; fi
sh -n sfc-app.sh
echo 'SFC offline ARM execution/visible-changing-pixels/browser/invalid-argument PASS'
