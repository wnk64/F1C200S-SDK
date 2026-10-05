#!/bin/sh
set -eu
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
test_dir=$(mktemp -d /root/roms/.reader-memory-test.XXXXXX)
cleanup() {
    rm -f "$test_dir/state"
    rmdir "$test_dir/space directory" "$test_dir"
}
trap cleanup EXIT
trap 'exit 1' INT TERM
mkdir "$test_dir/space directory"
export READER_PATH_STATE="$test_dir/state"
unset READER_START_DIR READER_SELFTEST READER_TEST_TIMER_UI
expected="$test_dir/space directory"
"$SELF/reader-browser" "$expected" --list
[ "$(cat "$READER_PATH_STATE")" = "$expected" ]
inode=$(ls -i "$READER_PATH_STATE" | awk '{ print $1 }')
"$SELF/reader-browser" "$expected" --list
[ "$(ls -i "$READER_PATH_STATE" | awk '{ print $1 }')" = "$inode" ]
MEDIA_BROWSER_TEST_MS=300 "$SELF/reader-app.sh"
[ "$(cat "$READER_PATH_STATE")" = "$expected" ]
echo 'READER remembered directory restart/space/unchanged-write PASS'
for invalid in "$test_dir/deleted" relative/path; do
    printf '%s\n' "$invalid" > "$READER_PATH_STATE"
    MEDIA_BROWSER_TEST_MS=300 "$SELF/reader-app.sh"
    [ "$(cat "$READER_PATH_STATE")" = /root/roms ]
done
echo 'READER missing/relative directory fallback PASS'
rm -f "$READER_PATH_STATE"
MEDIA_BROWSER_TEST_MS=300 "$SELF/reader-app.sh"
[ "$(cat "$READER_PATH_STATE")" = /root/roms ]
MEDIA_BROWSER_TEST_MS=300 READER_START_DIR="$expected" "$SELF/reader-app.sh"
[ "$(cat "$READER_PATH_STATE")" = "$expected" ]
echo 'READER initial default/explicit override PASS'
READER_PATH_STATE="$test_dir/missing/state" "$SELF/reader-browser" "$expected" --list
echo 'READER write failure remains nonfatal PASS'
