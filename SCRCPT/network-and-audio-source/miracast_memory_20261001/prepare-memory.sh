#!/bin/sh
set -eu
PATH=/sbin:/usr/sbin:/bin:/usr/bin
STATE=/run/f1-miracast-memory
umask 077

status() {
    awk '/^(MemFree:|MemAvailable:|SUnreclaim:|Slab:|CmaFree:)/ {print}
         /^MemFree:/ {free=$2} /^CmaFree:/ {cma=$2}
         END {printf "Free outside CMA: %d kB\n", free-cma}' /proc/meminfo
    for pair in vivid:vivid.0 vimc:vimc.0 snd_dummy:snd_dummy.0 snd_aloop:snd_aloop.0; do
        driver=${pair%%:*}; device=${pair#*:}
        if [ -L "/sys/bus/platform/drivers/$driver/$device" ]; then
            echo "$pair bound"
        else
            echo "$pair unbound"
        fi
    done
}

check_unused() {
    set --
    for file in /dev/video* /dev/media* /dev/vbi* /dev/radio* /dev/swradio* /dev/v4l-*; do
        [ ! -e "$file" ] || set -- "$@" "$file"
    done
    # Card numbers change when the test drivers are removed from the kernel.
    for card in /sys/class/sound/card[0-9]*; do
        [ -r "$card/id" ] || continue
        case "$(cat "$card/id")" in
            Dummy|Loopback)
                name=${card##*/}; number=${name#card}
                for file in /dev/snd/controlC$number /dev/snd/pcmC${number}D* /dev/snd/hwC${number}D* /dev/snd/midiC${number}D*; do
                    [ ! -e "$file" ] || set -- "$@" "$file"
                done
                ;;
        esac
    done
    if [ "$#" -gt 0 ]; then
        rc=0
        busybox fuser -s "$@" || rc=$?
        case "$rc" in
            0) echo 'Refusing preparation: video or test sound device is open' >&2; exit 1 ;;
            1) ;;
            *) echo 'Unable to check device users' >&2; exit 1 ;;
        esac
    fi
}

prepare() {
    wanted=${1:-all}
    case "$wanted" in
        all|snd_dummy:snd_dummy.0|snd_aloop:snd_aloop.0|vimc:vimc.0|vivid:vivid.0) ;;
        *) echo 'Unknown preparation target' >&2; exit 1 ;;
    esac
    echo 'Checking device users before unbinding'
    check_unused
    mkdir -p "$STATE"
    [ -f "$STATE/devices" ] || : > "$STATE/devices"
    for pair in snd_dummy:snd_dummy.0 snd_aloop:snd_aloop.0 vimc:vimc.0 vivid:vivid.0; do
        [ "$wanted" = all ] || [ "$wanted" = "$pair" ] || continue
        driver=${pair%%:*}; device=${pair#*:}
        [ -L "/sys/bus/platform/drivers/$driver/$device" ] || continue
        # Record rollback before changing the kernel binding.
        grep -qx "$pair" "$STATE/devices" || printf '%s\n' "$pair" >> "$STATE/devices"
        printf '%s\n' "$device" > "/sys/bus/platform/drivers/$driver/unbind"
        echo "Unbound $pair"
        status
    done
    echo 'Preserved: Codec, Bluetooth/BlueALSA/D-Bus, BS Wi-Fi/SSH, DRM/Cedar and AIC'
}

restore() {
    [ -r "$STATE/devices" ] || return 0
    while IFS=: read -r driver device; do
        case "$driver:$device" in
            vivid:vivid.0|vimc:vimc.0|snd_dummy:snd_dummy.0|snd_aloop:snd_aloop.0) ;;
            *) echo 'Invalid rollback entry' >&2; exit 1 ;;
        esac
        [ -L "/sys/bus/platform/drivers/$driver/$device" ] && continue
        printf '%s\n' "$device" > "/sys/bus/platform/drivers/$driver/bind"
    done < "$STATE/devices"
    rm -f "$STATE/devices"
    rmdir "$STATE"
    status
}

case "${1:-prepare}" in
    prepare) prepare "${2:-all}" ;;
    restore) restore ;;
    status) status ;;
    *) echo "Usage:$0 {prepare [driver:device]|restore|status}" >&2; exit 1 ;;
esac
