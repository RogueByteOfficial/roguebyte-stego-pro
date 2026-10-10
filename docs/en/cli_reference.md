# CLI Reference — RogueByte Stego Pro

**Author**: SidoDev-Dz | **Organization**: RogueByte

## Command Synopsis

```bash
rb-stego <command> [options]
```

### Commands

1. **`embed`**: Embeds payload(s) into a cover image.
   ```bash
   rb-stego embed --cover <path> --input <path>... --output <path> [options]
   ```
   - `--cover <path>`: Cover image path (`.png`, `.bmp`, `.jpg`).
   - `--input <path>`: Payload file to embed. Can be specified multiple times to bundle multiple files into a container manifest.
   - `--output <path>`: Output stego image path.
   - `--encrypt`: Enables AES-256-GCM encryption.
   - `--password <pwd>`: Optional inline password (if omitted, secure password prompt is displayed without terminal echo).
   - `--iterations <n>`: PBKDF2 iterations (default: 100000).
   - `--force`: Overwrite existing output file.

2. **`extract`**: Extracts payload(s) from a stego carrier.
   ```bash
   rb-stego extract --input <path> --output-dir <dir> [options]
   ```
   - `--input <path>`: Stego carrier image.
   - `--output-dir <dir>`: Destination folder. Rejects directory traversal paths.
   - `--password <pwd>`: Decryption password (prompted if needed).
   - `--force`: Overwrite files in destination folder.

3. **`inspect`**: Read-only forensic analysis of cover or stego image.
   ```bash
   rb-stego inspect --input <path> [--json]
   ```
   - Reports dimensions, channel depth, raw & usable capacities, bitplane Shannon entropy, Chi-square distribution, RogueByte container header verification, and confidence verdict.

4. **`verify`**: Cryptographic file verification.
   ```bash
   rb-stego verify --input <path> [--expected <sha256>]
   ```
   - Computes SHA-256 and compares against optional expected hash.

5. **`capacity`**: Calculates maximum steganographic payload size for an image.
   ```bash
   rb-stego capacity --cover <path>
   ```

6. **`version`**: Prints build and version information.
   ```bash
   rb-stego version
   ```

---

## Exit Codes

| Exit Code | Meaning |
|---|---|
| `0` | Success |
| `1` | General Error |
| `2` | Invalid Arguments |
| `3` | Cover File Error (missing, corrupt, unsupported format) |
| `4` | Payload Error (missing or unreadable input file) |
| `5` | Insufficient Capacity |
| `6` | Authentication Failure (wrong password or AEAD tag mismatch) |
| `7` | Integrity Verification Failure (SHA-256 checksum mismatch) |
| `8` | Security Violation (path traversal attempt detected) |
