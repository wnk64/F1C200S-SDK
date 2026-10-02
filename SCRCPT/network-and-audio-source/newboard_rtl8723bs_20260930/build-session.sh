#!/bin/bash
HERE=$(cd "$(dirname "$0")" && pwd)
bash "$HERE/build.sh" > /tmp/f1-rtl8723bs-0930.log 2>&1
rc=$?
printf '%s\n' "$rc" > /tmp/f1-rtl8723bs-0930.rc
exec bash
