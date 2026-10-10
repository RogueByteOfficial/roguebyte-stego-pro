#!/usr/bin/env bash
set -euo pipefail

echo "========================================================"
echo " Building RogueByte Stego Pro on Linux / Kali Linux"
echo " Author: SidoDev-Dz | Organization: RogueByte"
echo "========================================================"

BUILD_DIR="build/linux-release"

mkdir -p "$BUILD_DIR"
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "[+] Build complete: $BUILD_DIR/rb-stego"
"$BUILD_DIR/rb-stego" version
