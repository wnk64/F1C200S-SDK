#!/bin/sh
set -eu
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
UI=/root/roms/candidates/bluetooth_hid_20261010/f1-menu-hid
test_once=0
case "${1:-}" in
    '') ;;
    --test-cast-once) test_once=1 ;;
    *) echo 'usage: menu-supervisor.sh [--test-cast-once]' >&2; exit 1 ;;
esac
LOCK=/run/f1-menu.lock
mkdir "$LOCK" 2>/dev/null || { echo 'menu supervisor already active' >&2; exit 1; }
child=
CONSOLE=/root/roms/candidates/menu_console_20261005/f1-console-mode
console_mode=
cleanup() {
    trap - EXIT INT TERM
    if [ -n "$child" ]; then kill -TERM "$child" 2>/dev/null || true; wait "$child" 2>/dev/null || true; fi
    if [ -n "$console_mode" ]; then "$CONSOLE" "$console_mode" >/dev/null || true; fi
    rmdir "$LOCK"
}
trap cleanup EXIT
trap 'exit 0' INT TERM
# Keep fbcon suppressed across menu/app handoffs; children restore graphics.
console_mode=$("$CONSOLE" graphics)
error=
while :; do
    rc=0
    if [ -n "$error" ]; then
        F1_MENU_ERROR=1 "$UI" "$SELF/apps.conf" &
    else
        "$UI" "$SELF/apps.conf" &
    fi
    child=$!
    if [ "$test_once" -eq 1 ]; then
        sleep 2
        kill -TERM "$child" 2>/dev/null || true
    fi
    wait "$child" || rc=$?
    child=
    if [ "$test_once" -eq 1 ]; then
        test_once=0
        [ "$rc" -ne 0 ] || rc=20
    fi
    [ "$rc" -ne 0 ] || exit 0
    if [ "$rc" -eq 32 ] || [ "$rc" -eq 33 ]; then
        if [ "$rc" -eq 32 ] && [ ! -d /sys/module/f1_board_power ]; then
            echo 'Poweroff driver is not loaded' >&2
            error=1
            continue
        fi
        sync
        if [ "$rc" -eq 32 ]; then /sbin/poweroff || true; else /sbin/reboot || true; fi
        # Normal init shutdown stops this supervisor; recover UI if it never begins.
        sleep 20
        error=1
        continue
    fi
    if [ "$rc" -lt 20 ] || [ "$rc" -gt 31 ]; then
        echo "menu failed: $rc" >&2; exit "$rc"
    fi
    # UI process has been reaped: no LVGL heap or framebuffer mapping remains.
    index=$((rc - 20 + 1))
    command=$(awk -F '|' '!/^#/ && NF == 5 { n++; if (n == target) print $5 }' target="$index" "$SELF/apps.conf")
    case "$command" in /*) ;; *) echo 'invalid app command' >&2; exit 1 ;; esac
    app_rc=0
    "$command" >> "$SELF/application.log" 2>&1 &
    child=$!
    wait "$child" || app_rc=$?
    child=
    [ "$app_rc" -ne 2 ] || { echo 'application resources not released; UI remains stopped' >&2; exit 2; }
    if [ "$app_rc" -eq 0 ]; then error=; else error=1; fi
done
