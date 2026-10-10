#!/data/data/com.termux/files/usr/bin/bash
# RogueByte Stego Pro — Android Termux Build Script
# Author: SidoDev-Dz (RogueByte)

set -euo pipefail

echo "========================================================"
echo " Building RogueByte Stego Pro on Android Termux"
echo " Author: SidoDev-Dz | Organization: RogueByte"
echo "========================================================"

# Check and install required packages without root
if ! command -v clang++ &> /dev/null || ! command -v cmake &> /dev/null; then
    echo "[*] Updating Termux packages and installing clang, cmake, openssl..."
    pkg update -y
    pkg install -y clang cmake make openssl
fi

BUILD_DIR="build/termux-release"
mkdir -p "$BUILD_DIR"

cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_COMPILER=clang

cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "[+] Termux build succeeded!"
"$BUILD_DIR/rb-stego" version
