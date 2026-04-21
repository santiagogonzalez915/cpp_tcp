#!/bin/sh
# Convenience wrapper: run the NetBridge simulator.
# Usage: simulate.sh <num-nodes> <topo-file> [command-file] [timescale]
set -e
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
BIN="$HERE/../build/netstack"
exec "$BIN" simulate "$@"
