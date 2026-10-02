#!/bin/sh
set -eu
PATH=/sbin:/usr/sbin:/bin:/usr/bin
MEM_PREP=/root/aic_miracast/prepare-memory.sh
[ -x "$MEM_PREP" ] || { echo 'Missing Miracast memory preparation tool' >&2; exit 1; }
"$MEM_PREP" prepare
BASE=/root/aic_miracast/candidates
LOADER=$BASE/ugreen_pool64_disconnectfix_20260922/aic_load_fw.ko
FW_DIR=$BASE/ugreen_pool64_disconnectfix_20260922
FDRV=$BASE/ugreen_rxguard_db0d0b4_20260927/aic8800_fdrv.ko
check() {
    sum=$(md5sum "$1")
    [ "${sum%% *}" = "$2" ] || { echo "Unexpected artifact:$1" >&2; exit 1; }
}
find_iface() {
    for dev in /sys/class/net/wlan*; do
        [ -r "$dev/device/idVendor" ] && [ -r "$dev/device/idProduct" ] || continue
        if [ "$(cat "$dev/device/idVendor")" = a69c ] && [ "$(cat "$dev/device/idProduct")" = 8d83 ]; then
            IFACE=${dev##*/}; return 0
        fi
    done
    return 1
}
if ! find_iface; then
    check "$LOADER" bb2d68ed26fbb8d3cee554af0aa7735a
    check "$FW_DIR/fmacfw_8800d80_u02.bin" 01acfbebdfb15755e3fe853e7bc95c7d
    check "$FDRV" 334699cc17b7201a23c790306c64b381
    grep -q '^aic_load_fw ' /proc/modules || insmod "$LOADER" aic_fw_path="$FW_DIR" aicwf_dbg_level=0
    n=0
    until lsusb | grep -qi 'a69c:8d83'; do
        [ "$n" -lt 20 ] || { echo 'AIC firmware enumeration timeout' >&2; exit 1; }
        n=$((n + 1)); sleep 1
    done
    grep -q '^aic8800_fdrv ' /proc/modules || insmod "$FDRV" aicwf_dbg_level=0
    n=0
    until find_iface; do
        [ "$n" -lt 10 ] || { echo 'AIC interface unavailable' >&2; exit 1; }
        n=$((n + 1)); sleep 1
    done
fi
if [ "$IFACE" != wlan1 ]; then
    [ ! -e /sys/class/net/wlan1 ] || { echo 'wlan1 is already owned by another device' >&2; exit 1; }
    ip link set "$IFACE" down
    ip link set "$IFACE" name wlan1
fi
echo '[aic-miracast] wlan1 registered; no STA association, DHCP or route changes'
