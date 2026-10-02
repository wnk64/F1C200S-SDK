#!/bin/sh
set -eu
PATH=/sbin:/usr/sbin:/bin:/usr/bin
umask 077
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
R=/tmp/manual_miracast_20260920
P=/root/aic_miracast/candidates/miracast_stable_protocol_20260818
CLI=$P/wpa24_aic_wfd_cli
PLAYER=/root/aic_miracast/candidates/player_frame_gap_20261001/player-frame-gap
SINK=/root/aic_miracast/candidates/miracast_fifo_coalesce_20261001/sink-coalesce
BT=/org/bluez/hci0/dev_12_11_71_41_9C_4A

fail() { echo "cast-start: $*" >&2; exit 1; }
check() {
    [ -r "$1" ] || fail "missing artifact: $1"
    sum=$(md5sum "$1")
    [ "${sum%% *}" = "$2" ] || fail "artifact mismatch: $1"
}
preflight() {
    [ "$(uname -r)" = 5.7.1 ] || fail 'kernel release mismatch'
    [ "$(cat /sys/class/graphics/fb0/virtual_size)" = 480,800 ] || fail 'unexpected framebuffer'
    [ -e /sys/class/net/rtlwlan0 ] || fail 'missing BS control interface'
    check "$PLAYER" a683cf70a95ba2759b5c9d2b1e0dc016
    check "$SINK" 42d0cfa89120992a427abfde403770c9
    check "$P/wpa24_aic_wfd_supplicant" a9ae2b53fc4a3469fad24fd1abbd42fb
    check "$CLI" 97f7384a530c9514f75533ab9e530a56
    check "$P/tiny_dhcpd_49" 2e8c7e6533eeb85095799092da06707f
    check /root/aic_miracast/load-aic-for-miracast.sh dec5f2345c106b63182a9f1038e5cc86
    check /root/aic_miracast/prepare-memory.sh 0dcd3f8753f4f603267c319f31f50e54
    check /root/aic_miracast/candidates/ugreen_pool64_disconnectfix_20260922/aic_load_fw.ko bb2d68ed26fbb8d3cee554af0aa7735a
    check /root/aic_miracast/candidates/ugreen_pool64_disconnectfix_20260922/fmacfw_8800d80_u02.bin 01acfbebdfb15755e3fe853e7bc95c7d
    check /root/aic_miracast/candidates/ugreen_rxguard_db0d0b4_20260927/aic8800_fdrv.ko 334699cc17b7201a23c790306c64b381
    check "$SELF/wpa.conf" 300e8b756b24d8e3e9e0288e7b8e4f54
    check "$SELF/pbc-watch.sh" 32a3fa3d7f7d100a38364299cda62e08
    check "$SELF/rtsp-watch.sh" 2ad42f12091f64f548df07dc36fe9e8f
    for cmd in ip dbus-send; do command -v "$cmd" >/dev/null || fail "missing $cmd"; done
    echo 'cast-start: artifact preflight passed (read-only)'
}
cli() { "$CLI" -p "$R/wpa_ctrl" -i wlan1 "$@"; }
ok() { result=$(cli "$@"); [ "$result" = OK ] || fail "WPA command failed: $* / $result"; }
wait_old() {
    for file in player.pid sink.pid rtsp-watch.pid; do
        [ -r "$R/$file" ] || continue
        pid=$(cat "$R/$file")
        case "$pid" in ''|*[!0-9]*) fail "invalid PID in $file" ;; esac
        n=0
        while [ -d "/proc/$pid" ]; do
            n=$((n + 1)); [ "$n" -lt 20 ] || fail "old process still live: $pid"
            sleep 1
        done
    done
}
bt_connected() {
    dbus-send --system --print-reply --dest=org.bluez "$BT" org.freedesktop.DBus.Properties.Get string:org.bluez.Device1 string:Connected 2>/dev/null | grep -q 'boolean true'
}
ensure_audio() {
    if ! bt_connected; then
        dbus-send --system --print-reply --dest=org.bluez "$BT" org.bluez.Device1.Connect > "$R/bt-connect.log" 2>&1 || fail 'Bluetooth connection failed'
    fi
    n=0
    until grep -qx 'bluetooth 12:11:71:41:9C:4A' /run/audio-route.state 2>/dev/null; do
        n=$((n + 1)); [ "$n" -lt 15 ] || fail 'Bluetooth default route unavailable'
        sleep 1
    done
    bt_connected || fail 'Bluetooth disconnected during route setup'
}
check_go() {
    cli status > "$R/go-status.log"
    grep -qx 'wpa_state=COMPLETED' "$R/go-status.log" &&
    grep -qx 'mode=P2P GO' "$R/go-status.log" &&
    grep -qx 'freq=5745' "$R/go-status.log"
}
cold() {
    [ ! -e "$R" ] || fail 'cold mode requires a fresh boot/runtime; refusing overwrite'
    /root/aic_miracast/load-aic-for-miracast.sh
    [ -e /sys/class/net/wlan1 ] || fail 'AIC wlan1 unavailable'
    mkdir -p "$R/wpa_ctrl"
    cp "$SELF/wpa.conf" "$R/wpa.conf"
    cp "$SELF/pbc-watch.sh" "$R/pbc-watch.sh"
    cp "$SELF/rtsp-watch.sh" "$R/rtsp-watch.sh"
    ip link set wlan1 up
    ip addr replace 192.168.49.1/24 dev wlan1
    "$P/wpa24_aic_wfd_supplicant" -B -Dnl80211 -i wlan1 -c "$R/wpa.conf" -f "$R/wpa.log" -P "$R/wpa.pid"
    n=0
    until cli ping 2>/dev/null | grep -qx PONG; do
        n=$((n + 1)); [ "$n" -lt 10 ] || fail 'WPA control unavailable'; sleep 1
    done
    ok set wifi_display 1
    ok wfd_subelem_set 0 000600111c44012c
    ok wfd_subelem_set 1 0006000000000000
    ok wfd_subelem_set 6 000700000000000000
    ok wfd_subelem_set 11 00020001
    ok p2p_stop_find
    ok p2p_group_add freq=5745
    n=0
    until check_go; do
        n=$((n + 1)); [ "$n" -lt 10 ] || fail 'GO5745 unavailable'; sleep 1
    done
    "$P/tiny_dhcpd_49" wlan1 > "$R/dhcp.log" 2>&1 < /dev/null &
    printf '%s\n' "$!" > "$R/dhcp.pid"
    sh "$R/pbc-watch.sh" > "$R/pbc-watch.stdout" 2>&1 < /dev/null &
    printf '%s\n' "$!" > "$R/pbc-watch.pid"
    [ ! -e /tmp/aic_h264_live.fifo ] || fail 'unexpected existing FIFO'
    mkfifo /tmp/aic_h264_live.fifo
}
session() {
    wait_old
    check_go || fail 'existing GO5745 unavailable'
    check "$R/pbc-watch.sh" 32a3fa3d7f7d100a38364299cda62e08
    check "$R/rtsp-watch.sh" 2ad42f12091f64f548df07dc36fe9e8f
    [ -p /tmp/aic_h264_live.fifo ] || fail 'live FIFO unavailable'
    ensure_audio
    env CEDAR_FRAME_GAP_STATS=1 CEDAR_CMA_POOL_MB=9 CEDAR_ROTATE=90 CEDAR_AUDIODEV=default "$PLAYER" --raw-h264 800 480 60 /tmp/aic_h264_live.fifo > "$R/player-frame-gap.log" 2>&1 < /dev/null &
    printf '%s\n' "$!" > "$R/player.pid"
    env WFD_VIDEO_COALESCE=1 WFD_RTP_BATCH=1 WFD_LOSS_IDR=1 WFD_SINK="$SINK" sh "$R/rtsp-watch.sh" > "$R/watch-coalesce-launch.log" 2>&1 < /dev/null &
    printf '%s\n' "$!" > "$R/rtsp-watch.pid"
    sleep 2
    kill -0 "$(cat "$R/player.pid")"
    kill -0 "$(cat "$R/rtsp-watch.pid")"
    echo "cast-start: ready for phone, boot=$(cat /proc/sys/kernel/random/boot_id)"
}
case "${1:-preflight}" in
    preflight) preflight ;;
    cold) preflight; cold; session ;;
    session) preflight; session ;;
    *) fail 'usage: start-cast.sh {preflight|cold|session}' ;;
esac
