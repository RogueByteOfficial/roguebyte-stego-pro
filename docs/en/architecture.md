# Architecture and Container Format Design

**Project**: RogueByte Stego Pro  
**Author**: SidoDev-Dz  
**Organization**: RogueByte  
**Language**: C++20  

---

## 1. System Overview

RogueByte Stego Pro is designed around modular, decoupled subsystems with zero unnecessary runtime dependencies:

```
┌────────────────────────────────────────────────────────┐
│                   CLI Entry Point                      │
│                      (rb-stego)                        │
└──────────────────────────┬─────────────────────────────┘
                           │
       ┌───────────────────┼────────────────────┐
       ▼                   ▼                    ▼
┌──────────────┐   ┌───────────────┐   ┌─────────────────┐
│ Crypto Engine│   │ Container /   │   │ Image Backends  │
│ (OpenSSL 3.0)│   │ Manifest Hub  │   │(PNG, BMP, JPEG) │
└──────┬───────┘   └───────┬───────┘   └────────┬────────┘
       │                   │                    │
       └───────────────────┼────────────────────┘
                           ▼
               ┌───────────────────────┐
               │  Forensic Steganalysis│
               │  (Entropy, PoV Chi^2) │
               └───────────────────────┘
```

---

## 2. RogueByte Container Format Specification (RBS1)

The container format is explicitly versioned, bound-checked, and authenticated.

### 2.1 Binary Layout (112-Byte Fixed Header)

| Offset (Hex) | Size (Bytes) | Field Name | Type / Encoding | Description |
|---|---|---|---|---|
| `0x00` | 4 | `magic` | ASCII `RBS1` | Container identifier signature (`0x52 0x42 0x53 0x31`) |
| `0x04` | 1 | `version` | `uint8_t` | Container format version (currently `0x01`) |
| `0x05` | 1 | `flags` | `uint8_t` (Bitfield) | `0x01` = Encrypted, `0x02` = Compressed, `0x04` = Multi-File Manifest |
| `0x06` | 2 | `reserved` | `uint16_t` | Reserved for alignment, strictly `0x0000` |
| `0x08` | 4 | `kdf_iterations` | `uint32_t` LE | PBKDF2 iteration count (default: 100,000) |
| `0x0C` | 32 | `salt` | Binary | Cryptographic salt generated via OpenSSL `RAND_bytes` |
| `0x2C` | 12 | `nonce` | Binary | 96-bit initialization vector / IV for AES-256-GCM |
| `0x38` | 16 | `tag` | Binary | 128-bit Polyval / GHASH AEAD authentication tag |
| `0x48` | 8 | `payload_size` | `uint64_t` LE | Byte length of following payload data |
| `0x50` | 32 | `payload_sha256` | Binary | SHA-256 checksum of original unencrypted payload |
| `0x70` | $N$ | `payload_data` | Binary | Ciphertext (if encrypted) or raw data bytes |

---

## 3. Archive Manifest Specification (RBM1)

Multi-payload archiving allows embedding arbitrary binary collections (ZIP, PDF, EXE, DLL, APK, source files):

```
[0x00..0x03] Magic: 'RBM1' (4 bytes)
[0x04..0x07] Total Files Count (uint32_t LE)
For each entry:
  ├── [2 bytes] Filename Length (uint16_t LE, max 255)
  ├── [N bytes] Filename UTF-8 (Strictly Sanitized Basename)
  ├── [8 bytes] Original File Size (uint64_t LE)
  ├── [8 bytes] Unix Modification Timestamp (uint64_t LE)
  ├── [32 bytes] SHA-256 Hash of original file
  └── [M bytes] File Payload Binary Data
```

---

## 4. Cryptographic Specifications

- **AEAD Construction**: AES-256 in Galois/Counter Mode (GCM).
- **Key Derivation Function**: `PBKDF2-HMAC-SHA256` with 100,000 iterations and 256-bit random salt.
- **Integrity**: AEAD tag authentication verified before plaintext processing; SHA-256 recalculated and checked post-decryption.
- **Memory Security**: Wiping buffers using `OPENSSL_cleanse` immediately after key derivation and cryptographic completion.
