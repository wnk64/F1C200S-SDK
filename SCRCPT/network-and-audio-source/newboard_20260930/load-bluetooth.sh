#!/bin/sh
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
[ "$(uname -r)" = 5.7.1 ] || { echo 'Requires kernel5.7.1.' >&2; exit 1; }
[ -d /sys/bus/serial/devices ] || { echo 'Requires the serdev-enabled candidate kernel.' >&2; exit 1; }
found=0
for device in /sys/bus/serial/devices/*; do
    [ -f "$device/of_node/compatible" ] || continue
    tr '\000' '\n' < "$device/of_node/compatible" | grep -qx 'realtek,rtl8723ds-bt' && found=1
done
[ "$found" = 1 ] || { echo 'RTL8723DS Bluetooth DT node not registered.' >&2; exit 1; }
for firmware in rtl8723ds_fw.bin rtl8723ds_config.bin; do
    [ -s "/lib/firmware/rtl_bt/$firmware" ] || { echo "Missing firmware:$firmware" >&2; exit 1; }
done
[ ! -d /sys/module/hci_uart ] || { echo 'hci_uart already loaded; do not replace it live.' >&2; exit 1; }
[ -d /sys/module/btrtl ] || insmod "$HERE/btrtl.ko"
insmod "$HERE/hci_uart.ko"
echo 'H5 driver registered. Check dmesg and /sys/class/bluetooth for initialization.'
echo 'Do not run hciattach/rtk_hciattach: serdev owns UART2.'
