#!/bin/sh
set -eu
CONSOLE=/root/roms/candidates/menu_console_20261005/f1-console-mode
MENU=/root/aic_miracast/candidates/menu_graphics_boot_20261003
READER=/root/roms/candidates/reader_fullscreen_20261005
child=
original=
cleanup() {
    if [ -n "$child" ]; then kill -TERM "$child" 2>/dev/null || true; wait "$child" 2>/dev/null || true; fi
    if [ -n "$original" ]; then "$CONSOLE" "$original" >/dev/null; fi
    /etc/init.d/S90f1-menu start
}
trap cleanup EXIT
trap 'exit 1' INT TERM
[ "$("$CONSOLE" status)" = 1 ]
/etc/init.d/S90f1-menu stop
[ "$("$CONSOLE" status)" = 0 ]
echo 'CONSOLE service graphics/stop restores text PASS'
original=$("$CONSOLE" graphics)
for round in 1 2 3; do
    "$MENU/f1-menu-ip" "$MENU/apps.conf" & child=$!
    sleep 1
    kill -TERM "$child"
    wait "$child" || true
    child=
    [ "$("$CONSOLE" status)" = 1 ]
    MEDIA_BROWSER_TEST_MS=300 "$READER/reader-browser" /root/roms/reader_samples >/dev/null
    [ "$("$CONSOLE" status)" = 1 ]
    READER_SELFTEST=1 "$READER/reader" /root/roms/reader_samples/sample_utf8.txt
    [ "$("$CONSOLE" status)" = 1 ]
    MEDIA_BROWSER_TEST_MS=300 /root/aic_miracast/candidates/mplayer_menu_20261005/media-browser /root/roms >/dev/null
    [ "$("$CONSOLE" status)" = 1 ]
    echo "CONSOLE menu/reader browser/viewer/media browser handoff round $round PASS"
done
