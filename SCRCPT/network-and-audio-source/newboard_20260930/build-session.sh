#!/bin/bash
# Durable tmux entry point; retains a numeric status and an inspectable shell.
HERE=$(cd "$(dirname "$0")" && pwd)
bash "$HERE/build.sh" > /tmp/f1-rtl8723ds-0930.log 2>&1
rc=$?
printf '%s\n' "$rc" > /tmp/f1-rtl8723ds-0930.rc
exec bash
