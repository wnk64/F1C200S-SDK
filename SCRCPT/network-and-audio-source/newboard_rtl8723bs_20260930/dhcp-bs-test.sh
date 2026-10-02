#!/bin/sh
PATH=/usr/sbin:/usr/bin:/sbin:/bin
case "$1" in
    bound|renew)
        [ "$interface" = rtlwlan0 ] || exit 1
        ifconfig "$interface" "$ip" netmask "${subnet:-255.255.255.0}" up
        echo "[bs-dhcp] address=$ip subnet=${subnet:-255.255.255.0}; AIC default route and DNS retained"
        ;;
esac
