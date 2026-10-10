#!/usr/bin/env bash
set -euo pipefail

echo "========================================================"
echo " Executing RogueByte Stego Pro Test Suite"
echo "========================================================"

BUILD_DIR="${1:-build}"

if [ ! -d "$BUILD_DIR" ]; then
    echo "[*] Configuring and building tests..."
    cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" -j"$(nproc)"
fi

echo "[*] Running CTest..."
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo "[+] All unit and integration tests executed and verified successfully."
