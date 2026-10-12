#!/bin/sh
set -eu
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
UI=/opt/f1/apps/menu/f1-menu
RUNNER=$SELF/f1-app-runner
RELEASE=$SELF/app-release.sh
test -x "$RUNNER" && test -x "$RELEASE"
test_once=0
test_app=
case "${1:-}" in
    '') ;;
    --test-cast-once) test_once=1 ;;
    --test-app-once) test_once=1; test_app=${2:?app id required} ;;
    *) echo 'usage: menu-supervisor.sh [--test-cast-once]' >&2; exit 1 ;;
esac
LOCK=/run/f1-menu.lock
mkdir "$LOCK" 2>/dev/null || { echo 'menu supervisor already active' >&2; exit 1; }
child=
application_active=0
CONSOLE=/opt/f1/apps/menu/f1-console-mode
console_mode=
cleanup() {
    trap - EXIT INT TERM
    if [ -n "$child" ]; then kill -TERM "$child" 2>/dev/null || true; wait "$child" 2>/dev/null || true; fi
    if [ "$application_active" -eq 1 ]; then "$RELEASE" "$command" || echo 'Application exit hook failed' >&2; fi
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
        if [ -n "$test_app" ]; then
            rc=$(awk -F '|' '!/^#/ && NF == 5 {n++; if($1==target)print n+19}' target="$test_app" "$SELF/apps.conf")
            [ -n "$rc" ] || { echo 'Unknown test app' >&2; exit 1; }
        fi
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
    application_active=1
    "$RUNNER" "$command" >> "$SELF/application.log" 2>&1 &
    child=$!
    wait "$child" || app_rc=$?
    child=
    [ "$app_rc" -ne 125 ] || { echo 'application descendants not released; UI remains stopped' >&2; exit 2; }
    "$RELEASE" "$command" >> "$SELF/application.log" 2>&1 || { echo 'application resources not released; UI remains stopped' >&2; exit 2; }
    application_active=0
    if [ "$app_rc" -eq 0 ]; then error=; else error=1; fi
done
