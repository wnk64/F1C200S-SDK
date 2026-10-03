#!/bin/sh
set -eu
self=$(CDPATH= cd "$(dirname "$0")" && pwd)
export F1_WILI_EXEC="$self/wiliwili-controls-lvgl"
export F1_CEDAR_PLAYER="$self/cedar-controls-player"
exec "$self/launch-net-with-menu.sh" "$@"
