# Cross-Platform Build Guide

**Author**: SidoDev-Dz | **Organization**: RogueByte

## Prerequisites

- **C++ Compiler**: GCC 11+, Clang 13+, or MSVC 2022 supporting C++20.
- **CMake**: 3.20 or newer.
- **OpenSSL**: OpenSSL 3.0+ development headers (`libssl-dev` or Windows OpenSSL binary).

---

## 1. Linux & Kali Linux

```bash
# Ubuntu / Debian / Kali Linux:
sudo apt-get update
sudo apt-get install -y build-essential cmake libssl-dev

# Build using CMake:
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run tests:
ctest --test-dir build --output-on-failure
```

---

## 2. Android Termux (No Root Required)

```bash
# Inside Termux app:
pkg update
pkg install -y clang cmake make openssl

# Build:
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
cmake --build build -j$(nproc)

# Test:
ctest --test-dir build --output-on-failure
```

---

## 3. Windows 10/11 x64

```cmd
:: Using Visual Studio 2022 Command Prompt:
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

:: Run tests:
ctest --test-dir build -C Release --output-on-failure
```
