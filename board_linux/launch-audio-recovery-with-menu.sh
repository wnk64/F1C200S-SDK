#!/bin/sh
set -eu
self=$(CDPATH= cd "$(dirname "$0")" && pwd)
unset F1_DIAGNOSTIC_NO_OSD F1_AUDIO_RECOVERY_TEST F1_AUDIO_EAGAIN_TEST
export F1_WILI_EXEC="$self/wiliwili-audio-recovery"
export F1_CEDAR_PLAYER="$self/cedar-audio-backpressure"
export F1_PLAYER_DEBUG="${F1_PLAYER_DEBUG:-0}"
export CEDAR_FRAME_GAP_STATS="${CEDAR_FRAME_GAP_STATS:-0}"
exec "$self/launch-net-with-menu.sh" "$@"
