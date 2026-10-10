# RogueByte Stego Pro

[![C++20](https://img.shields.io/badge/Language-C%2B%2B20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/Build-CMake%203.20%2B-064F8C.svg)](https://cmake.org)
[![Security](https://img.shields.io/badge/AEAD-AES--256--GCM-green.svg)](https://www.openssl.org)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platforms](https://img.shields.io/badge/Platforms-Linux%20%7C%20Kali%20%7C%20Windows%20%7C%20Termux-purple.svg)](#supported-platforms)

**RogueByte Stego Pro** is a high-performance, cross-platform steganography and digital-forensics CLI suite developed in modern **C++20**. It allows secure embedding of arbitrary binary files inside lossless and structural carrier images with authenticated encryption (AEAD), exact integrity verification, and deep statistical digital forensics.

- **Author**: SidoDev-Dz
- **Organization**: RogueByte
- **Primary Language**: C++20

---
![roguebyte-stego-pro screenshot](screenshoot1.png)
![roguebyte-stego-pro screenshot](screenshoot2.png)
![roguebyte-stego-pro screenshot](screenshoot3.png)

## Key Features

1. **Arbitrary Binary Payload Support**:
   - Embed ZIP archives, PDF documents, TXT/Markdown files, executables (EXE/DLL), APKs, source code, and arbitrary byte streams.
   - Payloads are treated as untrusted binary data and never executed.
   - Preserves filenames, original sizes, modification timestamps, and SHA-256 hashes inside a serialized archive manifest (`RBM1`).

2. **Independent Image Carrier Backends**:
   - **PNG**: Lossless pixel-domain embedding using least significant bit (LSB) encoding on RGB channels (preserving alpha channel transparency).
   - **BMP**: Lossless pixel-domain embedding on uncompressed 24-bit and 32-bit DIB bitmaps with automatic scanline padding preservation.
   - **JPEG**: Structural application marker carrier steganography (APP15 / `RBS_STEG`) preserving all native DCT coefficients and quantization tables without lossy re-encoding artifacts.
   - Strict usable capacity calculation rejecting oversized payloads before file modifications occur.

3. **Military-Grade Cryptography & Container Architecture**:
   - **AEAD Construction**: AES-256-GCM authenticated encryption.
   - **Key Derivation**: PBKDF2-HMAC-SHA256 with 100,000 iterations and cryptographically random 256-bit salt.
   - **Tamper Resistance**: Any bit alteration triggers immediate AEAD authentication failure.
   - **Versioned 112-Byte Header (`RBS1`)**: Bounded parsing, explicit length validation, memory sanitization (`OPENSSL_cleanse`).

4. **Digital Forensic Steganalysis Engine**:
   - Structural image verification.
   - Bitplane Shannon entropy calculation for all 8 bit planes (Red, Green, Blue channels).
   - Pairs of Values (PoV) Chi-Square statistical test for LSB replacement anomalies.
   - Non-destructive RogueByte container detection.
   - Clear distinction between confirmed containers and heuristic anomaly findings.

5. **Security Hardening**:
   - Atomic output file creation via temporary swap files.
   - Path traversal defenses (rejecting and stripping `..`, absolute paths, and illegal characters).
   - Extraction strictly confined to designated destination directory.

---

## Supported Platforms

| Platform | Compiler / Toolchain | Build Status | Tested |
|---|---|---|---|
| **Linux x86_64** (Ubuntu / Debian) | GCC 12+ / Clang 14+ | Passed | Yes |
| **Kali Linux** | GCC / Clang | Passed | Yes |
| **Android Termux** | Clang Toolchain (No root required) | Passed | Yes |
| **Windows 10/11 x64** | MSVC 2022 / Clang / MinGW | Passed | Yes |

---

## Quick Start & Installation

### Linux & Kali Linux

```bash
# Install prerequisites
sudo apt-get update && sudo apt-get install -y build-essential cmake libssl-dev

# Clone and build
git clone https://github.com/SidoDev-Dz/roguebyte-stego-pro.git
cd roguebyte-stego-pro
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run tests
ctest --test-dir build --output-on-failure
```

### Android Termux (No Root)

```bash
pkg update && pkg install -y clang cmake make openssl
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

---

## CLI Usage & Commands

```bash
# 1. Check usable cover image capacity
./build/rb-stego capacity --cover cover.png

# 2. Embed arbitrary payload(s) with AES-256-GCM encryption
./build/rb-stego embed --cover cover.png --input confidential.zip --output stego.png --encrypt

# 3. Perform read-only forensic steganalysis on an image
./build/rb-stego inspect --input stego.png

# 4. Extract hidden files safely into an isolated directory
./build/rb-stego extract --input stego.png --output-dir ./recovered/

# 5. Verify cryptographic integrity against SHA-256
./build/rb-stego verify --input ./recovered/confidential.zip

# 6. Display version and architecture
./build/rb-stego version
```

---

## Repository Structure

```
├── CMakeLists.txt              # Primary CMake build file
├── CMakePresets.json           # Presets for Linux, Windows, Termux
├── include/roguebyte_stego/    # Public C++20 API headers
│   ├── common.hpp              # Exit codes, atomic I/O, sanitization
│   ├── crypto.hpp              # OpenSSL EVP AES-256-GCM, PBKDF2, SHA-256
│   ├── container.hpp           # RBS1 112-byte header serialization
│   ├── manifest.hpp            # Multi-file archive container format
│   ├── image_backend.hpp       # Abstract image backend base & factory
│   ├── png_backend.hpp         # Lossless PNG LSB backend
│   ├── bmp_backend.hpp         # Lossless BMP LSB backend
│   ├── jpeg_backend.hpp        # Structural JPEG segment backend
│   ├── forensics.hpp           # Forensic steganalysis engine
│   └── version.hpp             # Version and author metadata
├── src/
│   ├── core/                   # Common utilities & forensic engine
│   ├── crypto/                 # Cryptographic implementations
│   ├── container/              # Container & manifest logic
│   ├── image/                  # PNG, BMP, JPEG backends & LodePNG
│   └── cli/                    # CLI binary (main.cpp)
├── tests/
│   ├── unit/                   # Unit test suite
│   ├── integration/            # Round-trip and security tests
│   └── fixtures/               # Test covers and payloads
├── docs/
│   ├── en/                     # English documentation
│   └── ar/                     # Complete Arabic documentation
├── scripts/                    # Platform build and test scripts
├── .github/workflows/          # Continuous integration pipelines
├── LICENSE                     # MIT License
├── README.md                   # English Readme
├── README.ar.md                # Arabic Readme
├── SECURITY.md                 # Security and vulnerability disclosure
└── CONTRIBUTING.md             # Contribution guidelines
```

---

## License

Authored by **SidoDev-Dz** under the **RogueByte** organization. Distributed under the **MIT License**.
