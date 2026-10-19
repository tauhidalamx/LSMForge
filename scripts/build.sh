#!/usr/bin/env bash
set -euo pipefail

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build_${BUILD_TYPE,,}"

echo "========================================="
echo " Building LSMForge (${BUILD_TYPE})       "
echo "========================================="

if [ "${BUILD_TYPE,,}" = "debug" ]; then
    cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Debug -DLSMFORGE_ENABLE_ASAN=ON -DLSMFORGE_ENABLE_UBSAN=ON
else
    cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
fi

cmake --build "${BUILD_DIR}" -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"

echo "Build complete in: ${BUILD_DIR}"
