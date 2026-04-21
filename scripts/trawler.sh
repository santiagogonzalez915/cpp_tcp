#!/bin/sh
# Convenience wrapper: run the NetBridge trawler.
# Usage: trawler.sh <listen-port> [topo-file]
set -e
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
BIN="$HERE/../build/trawler"
exec "$BIN" "$@"
