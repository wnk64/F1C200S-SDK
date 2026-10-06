#!/bin/sh
set -eu
HELPER=/root/aic_miracast/candidates/menu_graphics_boot_20261003/connections-helper-ip
CLI=/root/aic_miracast/wpa24_aic_wfd_cli
KEY=776e6b3634315f322e3447
for round in 1 2 3; do
    "$HELPER" wifi-disconnect "$KEY"
    [ -f /run/rtl-bs-sta/manual-disconnect ]
    if /etc/init.d/S17rtl8723bs-sta recover; then exit 1; fi
    "$HELPER" wifi-connect "$KEY"
    "$CLI" -p /run/rtl-bs-sta -i rtlwlan0 status | grep -q '^wpa_state=COMPLETED'
    "$CLI" -p /run/rtl-bs-sta -i rtlwlan0 status | grep -q '^ip_address=10.0.0.234'
    [ ! -f /run/rtl-bs-sta/manual-disconnect ]
    if /etc/init.d/S17rtl8723bs-sta recover; then exit 1; fi
    echo "manual disconnect/reconnect/staticIPv4/no-unwanted-reset round $round PASS"
done
