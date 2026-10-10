#!/bin/sh
set -eu
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
cd "$SELF"
mkdir -p build/hid
gcc -std=gnu99 -O1 -g -Wall -Wextra -Wno-unused-function -fsanitize=address,undefined \
    -I. tests/test_evdev_hid.c -o build/hid/test-evdev
ASAN_OPTIONS=detect_leaks=1 build/hid/test-evdev
