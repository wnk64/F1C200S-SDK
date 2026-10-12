#!/bin/sh
set -eu
PATH=/sbin:/usr/sbin:/bin:/usr/bin
R=/tmp/manual_miracast_20260920
[ ! -L "$R" ] || { echo 'Cast runtime must not be a symlink' >&2; exit 1; }
command_of() {
    [ -r "/proc/$1/cmdline" ] || return 0
    tr '\000' ' ' < "/proc/$1/cmdline" 2>/dev/null || true
}
pid_from() {
    [ -r "$R/$1" ] || return 1
    value=$(cat "$R/$1")
    case "$value" in ''|*[!0-9]*) return 1 ;; esac
    [ "$value" -gt 1 ] || return 1
    printf '%s\n' "$value"
}
for file in player.pid sink.pid rtsp-watch.pid; do
    pid=$(pid_from "$file") || continue
    case "$(command_of "$pid")" in
        *'/opt/f1/apps/cast/cedar_drm_player'*|*'/opt/f1/apps/cast/miracast_sink_dump.lowest'*|*"$R/rtsp-watch.sh"*)
            echo 'Application cleanup refused: independent casting is still active' >&2
            exit 1 ;;
    esac
done
# Only descendants of verified cast helpers are eligible for termination.
terminate_tree() (
    root=$1
    [ -r "/proc/$root/stat" ] || exit 0
    stamp=$(awk '{print $22}' "/proc/$root/stat")
    for child in $(awk -v parent="$root" '/^Pid:/ {pid=$2} /^PPid:/ {if($2==parent)print pid}' /proc/[0-9]*/status 2>/dev/null); do
        terminate_tree "$child"
    done
    kill -TERM "$root" 2>/dev/null || true
    sleep 0.1
    if [ -r "/proc/$root/stat" ] && [ "$(awk '{print $22}' "/proc/$root/stat")" = "$stamp" ]; then
        kill -KILL "$root" 2>/dev/null || true
    fi
)
for file in dhcp.pid pbc-watch.pid wpa.pid; do
    pid=$(pid_from "$file") || continue
    command=$(command_of "$pid")
    case "$file:$command" in
        dhcp.pid:*'/opt/f1/apps/cast/cast-dhcp.sh wlan1'*|pbc-watch.pid:*"$R/pbc-watch.sh"*|wpa.pid:*'-i wlan1 '* )
            terminate_tree "$pid" ;;
    esac
done
# A missing PID file must not leave the exact wlan1 DHCP wrapper behind.
for status in /proc/[0-9]*/cmdline; do
    pid=${status#/proc/}; pid=${pid%/cmdline}
    case "$(command_of "$pid")" in
        '/bin/sh /opt/f1/apps/cast/cast-dhcp.sh wlan1 '*|'sh /opt/f1/apps/cast/cast-dhcp.sh wlan1 '*|'/usr/sbin/udhcpd -f /opt/f1/apps/cast/udhcpd.conf '*)
            terminate_tree "$pid" ;;
    esac
done
if [ -d /sys/module/aic8800_fdrv ]; then
    sleep 1
    rmmod aic8800_fdrv
fi
if [ -d /sys/module/aic_load_fw ]; then rmmod aic_load_fw; fi
# Retire only the known generated session files; preserve unknown contents.
if [ -d "$R" ]; then
    for name in dhcp.log dhcp.pid udhcpd.pid udhcpd.leases go-status.log pbc-action.log pbc-watch.pid pbc-watch.sh pbc-watch.stdout player-frame-gap.log player.pid rtsp-watch.log rtsp-watch.pid rtsp-watch.sh sink.log sink.pid watch-handoff-launch.log wpa.conf wpa.log wpa.pid; do
        rm -f "$R/$name"
    done
    [ ! -d "$R/wpa_ctrl" ] || rmdir "$R/wpa_ctrl"
    rmdir "$R"
fi
if [ -e /tmp/aic_h264_live.fifo ]; then
    [ ! -L /tmp/aic_h264_live.fifo ] && [ -p /tmp/aic_h264_live.fifo ] || exit 1
    rm -f /tmp/aic_h264_live.fifo
fi
echo 'Application cleanup: idle AIC resources released; BS Wi-Fi/SSH unchanged'
