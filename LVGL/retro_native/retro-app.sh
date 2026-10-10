#!/bin/sh
set -u
export F1_HIDE_MOUSE_CURSOR=1
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
case "${1:-}" in gba|ps1|md) system=$1;; *) exit 1;; esac
test -x "$SELF/retro-native" && test -x "$SELF/retro-browser" && test -r "$SELF/cores/$system.so" && test -r "$SELF/cjk28.bin" || exit 1
mkdir -p "$SELF/saves" "$SELF/bios"
export RETRO_SYSTEM_DIR="$SELF/bios" RETRO_SAVE_DIR="$SELF/saves"
selection=$(mktemp /tmp/f1-retro-selection.XXXXXX) || exit 1
child=
cleanup() {
    trap - EXIT INT TERM
    if [ -n "$child" ]; then kill -TERM "$child" 2>/dev/null || true; wait "$child" 2>/dev/null || true; fi
    rm -f "$selection"
}
trap cleanup EXIT
trap 'exit 0' INT TERM
browse=/root/roms/$system
if [ -r "$SELF/last-$system-directory" ]; then IFS= read -r saved <"$SELF/last-$system-directory"; [ ! -d "$saved" ] || browse=$saved; fi
[ -d "$browse" ] || browse=/root/roms
while :; do
    rc=0
    "$SELF/retro-browser" "$system" "$browse" >"$selection" & child=$!
    wait "$child" || rc=$?;child=
    [ "$rc" -ne 0 ] || exit 0
    [ "$rc" -eq 20 ] || exit "$rc"
    IFS= read -r rom <"$selection" || exit 1
    "$SELF/retro-browser" "$system" "$rom" --check || exit 1
    browse=$(dirname "$rom");printf '%s\n' "$browse" >"$SELF/last-$system-directory"
    "$SELF/retro-native" "$system" "$SELF/cores/$system.so" "$rom" >>"$SELF/$system.log" 2>&1 & child=$!
    rc=0;wait "$child" || rc=$?;child=
    [ "$rc" -eq 0 ] || exit "$rc"
done
