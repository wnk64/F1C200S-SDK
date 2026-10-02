#!/bin/sh
# Runtime-only: wpa_cli -a does not dispatch P2P events on this binary.
set -eu

R=/tmp/manual_miracast_20260920
CLI=/root/aic_miracast/candidates/miracast_stable_protocol_20260818/wpa24_aic_wfd_cli
LOG="$R/wpa.log"

test -r "$LOG"
test -x "$CLI"
printf 'PBC log watcher started\n' > "$R/pbc-action.log"

tail -n 0 -f "$LOG" | while IFS= read -r line; do
    case "$line" in
        P2P-PROV-DISC-PBC-REQ*)
            printf '%s event: %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$line" >> "$R/pbc-action.log"
            "$CLI" -p "$R/wpa_ctrl" -i wlan1 wps_pbc any >> "$R/pbc-action.log" 2>&1
            ;;
    esac
done
