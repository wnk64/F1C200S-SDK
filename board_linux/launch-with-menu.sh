#!/bin/sh
set -eu
unset F1_UI_TEST_MS
self=$(CDPATH= cd "$(dirname "$0")" && pwd)
test -f "$self/cacert.pem"
test -f "$self/cjk28.bin"
test -x "$self/wiliwili-lvgl"
ulimit -c 0
child=
from_menu=0
if [ -d /run/f1-menu.lock ]; then
    tr '\0' ' ' < "/proc/$PPID/cmdline" | grep -q 'menu-supervisor.sh' || { echo 'menu is still active' >&2; exit 1; }
    from_menu=1
fi
cleanup() {
    trap - INT TERM
    if [ -n "$child" ]; then kill -TERM "$child" 2>/dev/null || true; wait "$child" 2>/dev/null || true; fi
    exit 143
}
trap cleanup INT TERM
F1_CA_FILE="$self/cacert.pem" F1_CONFIG_DIR="$self/config" "$self/wiliwili-lvgl" "$@" &
child=$!
printf '%s\n' "$child" > "$self/ui-live.pid"
rc=0
wait "$child" || rc=$?
child=
trap - INT TERM
[ "$rc" = 0 ] || exit "$rc"
[ "$from_menu" = 0 ] || exit 0
exec sh /root/aic_miracast/candidates/cma_start_preflight_20261002/menu-supervisor.sh
