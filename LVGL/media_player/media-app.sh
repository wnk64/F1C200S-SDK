#!/bin/sh
set -u
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
BROWSER="$SELF/media-browser"
UI=/root/aic_miracast/candidates/mplayer_ui_20261005/mplayer-ui-lyrics-test
BACKEND=/root/aic_miracast/candidates/mplayer_ui_20261005/mplayer-cedar-ui
selection=$(mktemp /tmp/f1-media-selection.XXXXXX) || exit 1
child=
cleanup() {
    trap - EXIT INT TERM
    if [ -n "$child" ]; then kill -TERM "$child" 2>/dev/null || true; wait "$child" 2>/dev/null || true; fi
    rm -f "$selection"
}
trap cleanup EXIT
trap 'exit 0' INT TERM
test -x "$BROWSER" && test -x "$UI" && test -x "$BACKEND" || exit 1
browse=${MEDIA_APP_START_DIR:-/root/roms}
while :; do
    rc=0
    "$BROWSER" "$browse" > "$selection" & child=$!
    wait "$child" || rc=$?; child=
    [ "$rc" -ne 0 ] || exit 0
    [ "$rc" -eq 20 ] || exit "$rc"
    IFS= read -r media < "$selection" || exit 1
    "$BROWSER" "$media" --check || exit 1
    # Only one LVGL/display owner: browser is reaped before starting playback.
    "$UI" "$media" "$BACKEND" >> "$SELF/playback.log" 2>&1 & child=$!
    rc=0; wait "$child" || rc=$?; child=
    [ "$rc" -eq 0 ] || exit "$rc"
    [ "${MEDIA_APP_TEST_ONCE:-0}" != 1 ] || exit 0
    browse=$(dirname "$media")
done
