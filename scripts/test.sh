#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build_release}"

if [ ! -d "${BUILD_DIR}" ]; then
    echo "Directory ${BUILD_DIR} does not exist. Running build first..."
    ./scripts/build.sh Release
    BUILD_DIR="build_release"
fi

echo "========================================="
echo " Running LSMForge Test Suite (${BUILD_DIR}) "
echo "========================================="

ctest --test-dir "${BUILD_DIR}" --output-on-failure
