#!/bin/bash
set -euo pipefail
SERVICE=${1:?service source required}
test_root=$(mktemp -d)
trap 'rm -f "$test_root"/wpa.log "$test_root"/manual-disconnect "$test_root"/recover.last "$test_root"/called; rmdir "$test_root"/recover.lock 2>/dev/null || true; rmdir "$test_root"' EXIT
run_case() (
    eval "$(sed '/^case /,$d' "$SERVICE")"
    RUN=$test_root;CLI=fake_cli;ap=1;state=SCANNING
    fake_cli(){case "$*" in *status) echo "wpa_state=$state" ;; *scan_results) [ "$ap" -eq 0 ] || echo '50:4f:3b:cc:67:52 2412 -40 WPA2 test' ;; esac;}
    stop(){touch "$RUN/called";return 1;}
    rm -f "$RUN/manual-disconnect" "$RUN/recover.last" "$RUN/called"
    printf '%s\n' 'Trying to associate with 50:4f:3b:cc:67:52' 'ASSOC-REJECT status_code=1' 'ASSOC-REJECT status_code=1' > "$RUN/wpa.log"
    initial=0
    case "$1" in
        connected) state=COMPLETED ;;
        manual) touch "$RUN/manual-disconnect" ;;
        wrongkey) echo WRONG_KEY >> "$RUN/wpa.log" ;;
        stale) initial=2 ;;
        absent) ap=0 ;;
        cooldown) cut -d . -f1 /proc/uptime > "$RUN/recover.last" ;;
        qualify) ;;
    esac
    recover "$initial" && exit 1
    if [ "$1" = qualify ];then test -f "$RUN/called";else test ! -f "$RUN/called";fi
    echo "recovery gate $1 PASS"
)
for test_case in connected manual wrongkey stale absent cooldown qualify;do run_case "$test_case";done
