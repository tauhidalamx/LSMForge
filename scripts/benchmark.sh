#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build_release"
NUM_OPS="${1:-50000}"

if [ ! -f "${BUILD_DIR}/lsmforge_bench" ]; then
    echo "Benchmark executable not found. Building Release..."
    ./scripts/build.sh Release
fi

echo "========================================="
echo " Running LSMForge Microbenchmarks        "
echo "========================================="

"${BUILD_DIR}/lsmforge_bench" --num="${NUM_OPS}"
