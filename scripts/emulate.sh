#!/bin/sh
# Convenience wrapper: run an emulated NetBridge node.
# Usage: emulate.sh <trawler-host> <trawler-port> <local-udp-port> [command-file]
set -e
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
BIN="$HERE/../build/netstack"
exec "$BIN" emulate "$@"
