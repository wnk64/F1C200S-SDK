#!/bin/sh
set -eu
PATH=/usr/sbin:/usr/bin:/sbin:/bin
HERE=$(cd "$(dirname "$0")" && pwd)
[ "$(uname -r)" = 5.7.1 ] || { echo 'Requires kernel5.7.1' >&2; exit 1; }
found=0
for device in /sys/bus/serial/devices/*; do
    [ -f "$device/of_node/compatible" ] || continue
    tr '\000' '\n' < "$device/of_node/compatible" | grep -qx 'realtek,rtl8723bs-bt' && found=1
done
[ "$found" = 1 ] || { echo 'BS Bluetooth serdev node absent' >&2; exit 1; }
for firmware in rtl8723bs_fw.bin rtl8723bs_config.bin; do
    [ -s "/lib/firmware/rtl_bt/$firmware" ] || { echo "Missing firmware:$firmware" >&2; exit 1; }
done
[ ! -d /sys/module/hci_uart ] || { echo 'hci_uart already loaded; inspect before changing' >&2; exit 1; }
[ -d /sys/module/btrtl ] || insmod "$HERE/btrtl.ko"
insmod "$HERE/hci_uart.ko"
echo 'BS H5 driver registered; do not use hciattach on serdev-owned UART2.'
