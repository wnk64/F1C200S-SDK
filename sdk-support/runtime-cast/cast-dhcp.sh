#!/bin/sh
set -eu
PATH=/sbin:/usr/sbin:/bin:/usr/bin
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
if [ "${1:-}" = --parse-ack ]; then
    exec awk -f "$SELF/dhcp-ack.awk"
fi
[ "${1:-}" = wlan1 ] || { echo 'Cast DHCP requires wlan1' >&2; exit 1; }
test -x /usr/sbin/udhcpd
test -r "$SELF/udhcpd.conf"
test -r "$SELF/dhcp-ack.awk"
umask 077
temp=$(mktemp -d /tmp/f1-cast-dhcp.XXXXXX)
server=
reader=
cleanup() {
    trap - EXIT INT TERM
    if [ -n "$server" ]; then kill -TERM "$server" 2>/dev/null || true; wait "$server" 2>/dev/null || true; fi
    if [ -n "$reader" ]; then kill -TERM "$reader" 2>/dev/null || true; wait "$reader" 2>/dev/null || true; fi
    rm -f "$temp/log.fifo"
    rmdir "$temp"
}
trap cleanup EXIT
trap 'exit 143' INT TERM
mkfifo "$temp/log.fifo"
/usr/sbin/udhcpd -f "$SELF/udhcpd.conf" > "$temp/log.fifo" 2>&1 &
server=$!
awk -f "$SELF/dhcp-ack.awk" < "$temp/log.fifo" &
reader=$!
rc=0
wait "$server" || rc=$?
server=
exit "$rc"
