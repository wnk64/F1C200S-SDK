#!/bin/sh
set -eu
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
# The runner has reaped all application descendants before this hook runs.
"$SELF/release-idle-cast.sh"
