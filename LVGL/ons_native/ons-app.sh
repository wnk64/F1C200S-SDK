#!/bin/sh
set -u
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
PLAYER=/root/aic_miracast/candidates/ons_tremor_20261005/ons-lvgl-tremor-controls-rotated
BROWSER="$SELF/ons-rom-browser"
selection=$(mktemp /tmp/f1-ons-selection.XXXXXX) || exit 1
child=
cleanup() {
    trap - EXIT INT TERM
    if [ -n "$child" ]; then kill -TERM "$child" 2>/dev/null || true; wait "$child" 2>/dev/null || true; fi
    rm -f "$selection"
}
trap cleanup EXIT
trap 'exit 0' INT TERM
test -x "$PLAYER" && test -x "$BROWSER" || exit 1
browse=/root/roms/ons
while :; do
    rc=0
    "$BROWSER" "$browse" > "$selection" & child=$!
    wait "$child" || rc=$?
    child=
    [ "$rc" -ne 0 ] || exit 0
    [ "$rc" -eq 20 ] || exit "$rc"
    IFS= read -r game < "$selection" || exit 1
    "$BROWSER" "$game" --check || exit 1
    # The browser has exited: its LVGL heap, fonts and framebuffer are released.
    "$PLAYER" "$game" >> "$SELF/ons.log" 2>&1 & child=$!
    rc=0; wait "$child" || rc=$?; child=
    [ "$rc" -eq 0 ] || exit 1
    browse=$(dirname "$game")
done
