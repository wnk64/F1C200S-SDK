#!/bin/sh
set -eu
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
cd "$SELF"
test -r ./cjk28.bin || { echo 'Recorder CJK font asset missing' >&2; exit 1; }
exec ./mic-recorder
