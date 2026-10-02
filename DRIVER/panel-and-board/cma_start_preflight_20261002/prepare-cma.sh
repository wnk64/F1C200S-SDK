#!/bin/sh
set -eu
R=/tmp/manual_miracast_20260920
for file in player.pid sink.pid rtsp-watch.pid; do
    [ -r "$R/$file" ] || continue
    pid=$(cat "$R/$file")
    case "$pid" in ''|*[!0-9]*) echo 'CMA preparation: invalid PID' >&2; exit 1 ;; esac
    [ ! -d "/proc/$pid" ] || { echo "CMA preparation: active session $pid" >&2; exit 1; }
done
for process in /proc/[0-9]*; do
    exe=$(readlink "$process/exe" 2>/dev/null || true)
    case "$exe" in */f1-menu|*/lvgl-touch-test*) echo 'CMA preparation: UI still owns display' >&2; exit 1 ;; esac
done
echo 'CMA preparation before:'
grep -e '^MemFree:' -e '^MemAvailable:' -e '^Cached:' -e '^CmaFree:' /proc/meminfo
# Reclaim only unused file-cache pages while no decoder or menu is running.
sync
printf '1\n' > /proc/sys/vm/drop_caches
echo 'CMA preparation after:'
grep -e '^MemFree:' -e '^MemAvailable:' -e '^Cached:' -e '^CmaFree:' /proc/meminfo
