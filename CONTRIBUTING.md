# Contributing to RogueByte Stego Pro

Thank you for your interest in contributing to **RogueByte Stego Pro**, developed by **SidoDev-Dz** under the **RogueByte** organization.

## Development Standards

- **Language Standard**: Modern C++20 (`-std=c++20`).
- **Build System**: CMake 3.20+ with CTest integration.
- **Dependencies**: Keep external dependencies minimal. Cryptography is standardized on OpenSSL 3.0 EVP APIs. Image codecs are self-contained to enable seamless compilation across Windows, Linux, Kali Linux, and Android Termux without proprietary SDKs.
- **Code Style**:
  - Clear, defensive bounds checking.
  - No undefined behavior, no memory leaks.
  - All new features must include unit or integration tests under `tests/unit/` or `tests/integration/`.

## Workflow

1. Fork the repository and create a feature branch (`git checkout -b feature/awesome-backend`).
2. Run CMake and build:
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build -j$(nproc)
   ```
3. Run the full test suite:
   ```bash
   ctest --test-dir build --output-on-failure
   ```
4. Submit a Pull Request describing your changes, cryptographic implications, and test coverage.
