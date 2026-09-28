#!/usr/bin/env bash
# Runs both binaries and prints just the summaries side by side.
# Usage: ./scripts/compare.sh [iterations] [build_dir]
set -e

ITER="${1:-2000}"
BUILD_DIR="${2:-build}"

echo "############################################"
echo "# BASELINE  ($ITER iterations)"
echo "############################################"
"$BUILD_DIR/baseline" "$ITER" | tail -n 6

echo
echo "############################################"
echo "# IMPROVED  ($ITER iterations)"
echo "############################################"
"$BUILD_DIR/rt_version" "$ITER" | tail -n 7

echo
echo "Tip: for full per-cycle timestamps, run each binary directly, e.g.:"
echo "  $BUILD_DIR/baseline $ITER"
echo "  sudo chrt -f 80 $BUILD_DIR/rt_version $ITER   # actually get SCHED_FIFO"
