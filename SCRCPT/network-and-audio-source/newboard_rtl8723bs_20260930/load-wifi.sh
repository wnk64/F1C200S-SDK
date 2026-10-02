#!/bin/sh
set -eu
PATH=/usr/sbin:/usr/bin:/sbin:/bin
found=0
for device in /sys/bus/sdio/devices/*; do
    [ -f "$device/vendor" ] && [ -f "$device/device" ] || continue
    [ "$(cat "$device/vendor")" = 0x024c ] || continue
    case "$(cat "$device/device")" in
        0x0523|0x0525|0x0623|0x0626|0xb723) found=1 ;;
    esac
done
[ "$found" = 1 ] || { echo 'RTL8723BS SDIO device absent; no module loaded.' >&2; exit 1; }
[ ! -d /sys/module/8723ds ] || { echo 'DS driver already loaded; stop and inspect.' >&2; exit 1; }
[ ! -d /sys/module/r8723bs ] || { echo 'r8723bs already loaded'; exit 0; }
[ -s /lib/firmware/rtlwifi/rtl8723bs_nic.bin ] || { echo 'BS WLAN firmware missing' >&2; exit 1; }
MODULE=${1:-"$(dirname "$0")/r8723bs.ko"}
insmod "$MODULE" ifname=rtlwlan0 rtw_power_mgnt=0 rtw_ips_mode=0
