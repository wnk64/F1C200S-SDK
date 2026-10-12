#!/bin/sh
set -eu
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
# The runner has reaped all application descendants before this hook runs.
"$SELF/release-idle-cast.sh"
case "${1:-}" in
    /opt/f1/apps/wiliwili/*)
        if [ "${F1_KEEP_APP_LOGS:-0}" != 1 ]; then
            rm -f /run/wiliwili-ui.log /run/wiliwili-player.log
        fi ;;
esac
