#!/bin/sh
set -eu
R=/tmp/manual_miracast_20260920
LOG="$R/dhcp.log"
FIFO=/tmp/aic_h264_live.fifo
SINK=${WFD_SINK:-/root/aic_miracast/candidates/miracast_stall_observe_20261001/sink-observe}
test -r "$LOG"
test -p "$FIFO"
test -x "$SINK"
seen=$(wc -l < "$LOG")
printf 'watch start: existing DHCP lines=%s\n' "$seen" > "$R/rtsp-watch.log"
while :; do
    now=$(wc -l < "$LOG")
    if [ "$now" -gt "$seen" ] && sed -n "$((seen + 1)),$now p" "$LOG" | grep -q '^REQUEST '; then
        ip=$(sed -n "$((seen + 1)),$now p" "$LOG" | sed -n 's/^REQUEST .*ip=\([0-9.]*\)$/\1/p' | tail -n 1)
        test -n "$ip"
        printf 'DHCP REQUEST: %s\n' "$ip" >> "$R/rtsp-watch.log"
        WFD_VIDEO_ASYNC=1 WFD_AUDIO_ENABLE=1 WFD_AUDIODEV=default "$SINK" "$ip" "$FIFO" > "$R/sink.log" 2>&1 &
        pid=$!
        printf '%s\n' "$pid" > "$R/sink.pid"
        printf 'sink started: pid=%s ip=%s\n' "$pid" "$ip" >> "$R/rtsp-watch.log"
        rc=0
        wait "$pid" || rc=$?
        printf 'sink exited: rc=%s\n' "$rc" >> "$R/rtsp-watch.log"
        exit "$rc"
    fi
    seen=$now
    sleep 1
done
