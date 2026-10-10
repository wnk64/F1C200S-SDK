#!/bin/sh
set -u
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
selection=$(mktemp /tmp/f1-emulator-system.XXXXXX) || exit 1
child=
cleanup() {
    trap - EXIT INT TERM
    if [ -n "$child" ]; then kill -TERM "$child" 2>/dev/null || true;wait "$child" 2>/dev/null || true;fi
    rm -f "$selection"
}
trap cleanup EXIT
trap 'exit 0' INT TERM
while :;do
    rc=0
    "$SELF/retro-browser" --systems >"$selection" & child=$!
    wait "$child" || rc=$?;child=
    [ "$rc" -ne 0 ] || exit 0
    [ "$rc" -eq 20 ] || exit "$rc"
    IFS= read -r system <"$selection" || exit 1
    case "$system" in gba|ps1|md);;*)exit 1;;esac
    sh "$SELF/retro-app.sh" "$system" & child=$!
    rc=0;wait "$child" || rc=$?;child=
    [ "$rc" -eq 0 ] || exit "$rc"
done
