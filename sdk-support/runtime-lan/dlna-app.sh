#!/bin/sh
set -eu
PATH=/sbin:/usr/sbin:/bin:/usr/bin
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
"$SELF/release-idle-cast.sh"
cd "$SELF"
export LD_LIBRARY_PATH="$SELF/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec ./gmediarender -I rtlwlan0 -f F1C200S-DLNA -u f1c20050-2026-4000-8000-000000000001 -p 49494 -o cedar
