#!/bin/sh
set -u
export F1_HIDE_MOUSE_CURSOR=1
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
test -x "$SELF/sfc-native" && test -x "$SELF/sfc-browser" && test -r "$SELF/cjk28.bin" || exit 1
selection=$(mktemp /tmp/f1-sfc-selection.XXXXXX) || exit 1
child=
cleanup() {
    trap - EXIT INT TERM
    if [ -n "$child" ]; then kill -TERM "$child" 2>/dev/null || true; wait "$child" 2>/dev/null || true; fi
    rm -f "$selection"
}
trap cleanup EXIT
trap 'exit 0' INT TERM
browse=/root/roms/sfc
if [ -r "$SELF/last-directory" ]; then IFS= read -r saved < "$SELF/last-directory"; [ ! -d "$saved" ] || browse=$saved; fi
[ -d "$browse" ] || browse=/root/roms
while :; do
    rc=0
    "$SELF/sfc-browser" "$browse" >"$selection" & child=$!
    wait "$child" || rc=$?; child=
    [ "$rc" -ne 0 ] || exit 0
    [ "$rc" -eq 20 ] || exit "$rc"
    IFS= read -r rom <"$selection" || exit 1
    "$SELF/sfc-browser" "$rom" --check || exit 1
    browse=$(dirname "$rom")
    printf '%s\n' "$browse" > "$SELF/last-directory"
    "$SELF/sfc-native" "$rom" >>"$SELF/sfc.log" 2>&1 & child=$!
    rc=0;wait "$child" || rc=$?;child=
    [ "$rc" -eq 0 ] || exit "$rc"
done
