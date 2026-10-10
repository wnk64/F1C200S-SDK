#!/bin/sh
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
exec sh "$SELF/retro-app.sh" gba
