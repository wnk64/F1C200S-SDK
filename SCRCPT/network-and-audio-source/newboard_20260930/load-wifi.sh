#!/bin/sh
set -eu

# Run manually after the candidate DTB is selected and hardware is installed.
found=0
for device in /sys/bus/sdio/devices/*; do
    [ -f "$device/vendor" ] && [ -f "$device/device" ] || continue
    [ "$(cat "$device/vendor")" = 0x024c ] || continue
    case "$(cat "$device/device")" in
        0xd723|0xd724) found=1 ;;
    esac
done
[ "$found" = 1 ] || { echo 'RTL8723DS SDIO device absent; no module loaded.' >&2; exit 1; }
[ ! -d /sys/module/8723ds ] || { echo '8723ds is already loaded.'; exit 0; }
MODULE=${1:-"$(dirname "$0")/8723ds.ko"}
[ -f "$MODULE" ] || { echo "Module not found: $MODULE" >&2; exit 1; }
insmod "$MODULE" ifname=rtlwlan0 rtw_power_mgnt=0 rtw_ips_mode=0
echo 'Driver loaded with power saving disabled for initial bring-up.'
echo 'Check rtlwlan0; this tool does not change AIC association, IP or routing.'
