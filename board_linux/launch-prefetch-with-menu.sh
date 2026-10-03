#!/bin/sh
set -eu
self=$(CDPATH= cd "$(dirname "$0")" && pwd)
export F1_WILI_EXEC="$self/wiliwili-prefetch-lvgl"
export F1_CEDAR_PLAYER="$self/cedar-prefetch-player"
exec "$self/launch-net-with-menu.sh" "$@"
