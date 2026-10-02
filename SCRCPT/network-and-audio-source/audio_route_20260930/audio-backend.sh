#!/bin/sh
PATH=/usr/sbin:/usr/bin:/sbin:/bin
BA=/opt/f1-audio/bluealsa
owned_bt=
owned_ba=
cleanup() {
    [ -z "$owned_ba" ] || kill "$owned_ba" 2>/dev/null
    [ -z "$owned_bt" ] || kill "$owned_bt" 2>/dev/null
    exit 0
}
trap cleanup TERM INT
while :; do
    adapter=
    for path in /sys/class/bluetooth/hci*; do
        [ -d "$path" ] || continue
        adapter=${path##*/}
        break
    done
    if [ -n "$adapter" ]; then
        if ! pidof bluetoothd >/dev/null; then
            /usr/libexec/bluetooth/bluetoothd >>/run/audio-bluetoothd.log 2>&1 &
            owned_bt=$!
        fi
        if ! pidof bluealsa >/dev/null; then
            if hciconfig "$adapter" up >>/run/audio-bluealsa.log 2>&1; then
                "$BA" -i "$adapter" -p a2dp-source >>/run/audio-bluealsa.log 2>&1 &
                owned_ba=$!
            fi
        fi
    fi
    sleep 2 &
    wait $!
done
