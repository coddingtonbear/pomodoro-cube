#!/usr/bin/env bash
# Configure (first run only), build, and launch the simulator.
set -euo pipefail

cd "$(dirname "$0")"

BUILD_DIR="${BUILD_DIR:-build}"

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
  cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug
fi

cmake --build "$BUILD_DIR" -j "$(nproc)"
exec "$BUILD_DIR/sim" "$@"
