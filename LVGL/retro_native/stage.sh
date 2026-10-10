#!/bin/sh
set -eu
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
mkdir -p "$SELF/stage/cores" "$SELF/stage/bios" "$SELF/stage/saves" "$SELF/stage/licenses"
cp "$SELF/retro-native" "$SELF/retro-browser" "$SELF/"*-app.sh "$SELF/stage/"
cp "$SELF/build/cores/"*.so "$SELF/stage/cores/"
cp /home/wnk/Desktop/F1c200S_SOFT/LVGL/menu/cjk28.bin "$SELF/stage/cjk28.bin"
# gpSP includes its own open-source BIOS; no proprietary firmware is bundled.
cp "$SELF/vendor/gpsp/bios/README.md" "$SELF/stage/licenses/gpsp-open-bios.txt"
cp "$SELF/vendor/gpsp/COPYING" "$SELF/stage/licenses/gpsp-COPYING"
cp "$SELF/vendor/pcsx/COPYING" "$SELF/stage/licenses/pcsx-COPYING"
cp "$SELF/vendor/pico/COPYING" "$SELF/stage/licenses/picodrive-COPYING"
chmod +x "$SELF/stage/"*-app.sh "$SELF/stage/retro-native" "$SELF/stage/retro-browser"
find "$SELF/stage" -type f -exec sha256sum {} \;
