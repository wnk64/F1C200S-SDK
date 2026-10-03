#!/bin/sh
set -eu
self=$(CDPATH= cd "$(dirname "$0")" && pwd)
export F1_WILI_EXEC="$self/wiliwili-loop-lvgl"
export F1_CEDAR_PLAYER="$self/cedar-loop-player"
export F1_PLAYER_DEBUG="${F1_PLAYER_DEBUG:-0}"
export CEDAR_FRAME_GAP_STATS="${CEDAR_FRAME_GAP_STATS:-0}"
exec "$self/launch-net-with-menu.sh" "$@"
