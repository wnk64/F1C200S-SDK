#!/bin/sh
set -u
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
selection=$(mktemp /tmp/f1-reader-selection.XXXXXX) || exit 1
child=
cleanup() {
    trap - EXIT INT TERM
    if [ -n "$child" ]; then kill -TERM "$child" 2>/dev/null || true; wait "$child" 2>/dev/null || true; fi
    rm -f "$selection"
}
trap cleanup EXIT
trap 'exit 0' INT TERM
browse=${READER_START_DIR:-/root/roms}
while :; do
    rc=0
    "$SELF/reader-browser" "$browse" > "$selection" & child=$!
    wait "$child" || rc=$?; child=
    [ "$rc" -ne 0 ] || exit 0
    [ "$rc" -eq 20 ] || exit "$rc"
    IFS= read -r file < "$selection" || exit 1
    "$SELF/reader-browser" "$file" --check || exit 1
    "$SELF/reader" "$file" >> "$SELF/reader.log" 2>&1 & child=$!
    rc=0; wait "$child" || rc=$?; child=
    [ "$rc" -eq 0 ] || exit "$rc"
    [ "${READER_TEST_ONCE:-0}" != 1 ] || exit 0
    browse=$(dirname "$file")
done
