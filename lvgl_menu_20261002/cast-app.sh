#!/bin/sh
set -eu
START=/root/aic_miracast/candidates/audio_reopen_20261002/start-cast.sh
R=/tmp/manual_miracast_20260920
PLAYER=/root/aic_miracast/candidates/player_frame_gap_20261001/player-frame-gap
SINK=/root/aic_miracast/candidates/audio_reopen_20261002/sink-audio-reopen
owned=
same_process() {
    [ -r "/proc/$1/stat" ] && [ "$(awk '{print $3}' "/proc/$1/stat")" != Z ] &&
        [ "$(awk '{print $22}' "/proc/$1/stat")" = "$2" ]
}
remember() {
    file=$1
    [ -r "$R/$file" ] || return 0
    pid=$(cat "$R/$file")
    case "$pid" in ''|*[!0-9]*) return 0 ;; esac
    [ -r "/proc/$pid/stat" ] || return 0
    stamp=$(awk '{print $22}' "/proc/$pid/stat")
    owned="$owned $pid:$stamp"
}
cleanup() {
    trap - EXIT INT TERM
    for pair in $owned; do
        pid=${pair%%:*}; stamp=${pair#*:}
        if same_process "$pid" "$stamp"; then kill -TERM "$pid" 2>/dev/null || true; fi
    done
    # Do not recreate the UI until all owned display/stream processes exit.
    for pair in $owned; do
        pid=${pair%%:*}; stamp=${pair#*:}; n=0
        while same_process "$pid" "$stamp"; do
            n=$((n + 1))
            if [ "$n" -eq 15 ]; then kill -KILL "$pid" 2>/dev/null || true; fi
            if [ "$n" -gt 20 ]; then echo "cast cleanup blocked by $pid" >&2; exit 2; fi
            sleep 1
        done
    done
}
trap cleanup EXIT
trap 'exit 1' INT TERM
# Refuse to adopt or stop an independently started casting session.
for file in player.pid sink.pid rtsp-watch.pid; do
    [ -r "$R/$file" ] || continue
    pid=$(cat "$R/$file")
    case "$pid" in ''|*[!0-9]*) exit 1 ;; esac
    [ ! -d "/proc/$pid" ] || { echo 'independent cast still active' >&2; exit 1; }
done
mode=cold; [ ! -d "$R" ] || mode=session
rc=0
sh "$START" "$mode" || rc=$?
remember player.pid
remember rtsp-watch.pid
[ "$rc" -eq 0 ] || exit "$rc"
watch=$(cat "$R/rtsp-watch.pid")
watch_stamp=$(awk '{print $22}' "/proc/$watch/stat")
sink_seen=0
n=0
while same_process "$watch" "$watch_stamp"; do
    if [ "$sink_seen" -eq 0 ] && [ -r "$R/sink.pid" ]; then
        candidate=$(cat "$R/sink.pid")
        if [ "$(readlink "/proc/$candidate/exe" 2>/dev/null || true)" = "$SINK" ]; then
            remember sink.pid; sink_seen=1
        fi
    fi
    n=$((n + 1))
    if [ "$sink_seen" -eq 0 ] && [ "$n" -ge 180 ]; then echo 'phone connection timeout' >&2; exit 1; fi
    sleep 1
done
# A normal RTSP close and a failed handshake are reported separately.
grep -q 'sink exited: rc=0' "$R/rtsp-watch.log" || exit 1
