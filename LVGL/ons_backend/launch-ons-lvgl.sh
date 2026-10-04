#!/bin/sh
set -eu
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
test -x /root/onscripter-gbk
test -f "$SELF/libons-lvgl.so"
game=${1:-/root/roms/ons}
test -d "$game"
test -f "$game/default.ttf"
unset F1_UI_TEST_MS SDL_VIDEO_FBCON_ROTATION ONS_F1C200S_DRAW_MOUSE
export SDL_DISPLAY_W=640 SDL_AUDIODRIVER=alsa AUDIODEV=default
export SDL_VIDEODRIVER=dummy LD_PRELOAD="$SELF/libons-lvgl.so"
exec /root/onscripter-gbk --root "$game" --audiobuffer 8
