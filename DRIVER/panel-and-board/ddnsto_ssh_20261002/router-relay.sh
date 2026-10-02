#!/bin/sh
set -eu
BIN=/tmp/f1-websocat-arm64
test -x "$BIN"
test "$(sha256sum "$BIN" | cut -d ' ' -f1)" = 711a69576a2ff473fb01a90ffafb571c2ed019e55479d7ae71b12c2eadeb7011
exec "$BIN" --binary --exit-on-eof --buffer-size 65536 --restrict-uri /f1-ssh ws-l:127.0.0.1:18090 tcp:10.0.0.234:22
