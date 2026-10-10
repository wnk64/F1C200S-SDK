#!/bin/sh
set -eu
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
cd "$SELF"
OUT=build-display-bounded
mkdir -p "$OUT"
gcc -std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-parameter -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -I. -I/usr/include/libdrm tests/test_display_bounded.c utils/spsc_queue.c \
    -Wl,--gc-sections -lpthread -ldrm -o "$OUT/test-display"
ASAN_OPTIONS=detect_leaks=1 "$OUT/test-display"
gcc -std=gnu11 -O1 -g -ffunction-sections -fdata-sections -fsanitize=address,undefined \
    -I. -I/usr/include/libdrm tests/test_drm_lifecycle.c utils/spsc_queue.c utils/log.c \
    -Wl,--gc-sections -lpthread -ldrm -o "$OUT/test-lifecycle"
ASAN_OPTIONS=detect_leaks=1 "$OUT/test-lifecycle" 2>"$OUT/lifecycle.log"
gcc -std=gnu11 -O1 -g -fsanitize=address,undefined -Iutils tests/test_frame_gap.c -o "$OUT/test-frame-gap"
ASAN_OPTIONS=detect_leaks=1 "$OUT/test-frame-gap"
