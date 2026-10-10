# Security Policy — RogueByte Stego Pro

## Reporting Vulnerabilities

If you discover a security vulnerability in RogueByte Stego Pro, please report it privately to the security team at **security@roguebyte.dev** or contact **SidoDev-Dz**. Do NOT create public issues for undisclosed security vulnerabilities.

### Security Guarantees & Threat Model

1. **Untrusted Payloads**: Payloads embedded within or extracted from carriers are treated as untrusted raw binary data. RogueByte Stego Pro never executes, interprets, or evaluates extracted files.
2. **Path Traversal Defenses**: Extracted filenames from container manifests are strictly sanitized. Any path containing `..`, absolute separators (`/` or `\`), or non-printable control characters is sanitized to a clean basename. Extraction is confined to the specified directory.
3. **Cryptographic Integrity**: AES-256-GCM authenticated encryption (AEAD) is enforced when encryption is enabled. Any tampering with container headers, ciphertext bytes, or metadata results in immediate authentication failure (`ExitCode::AuthenticationFailed = 6`).
4. **Key Hygiene**: Passwords and derived encryption keys are zeroed from memory upon completion using `OPENSSL_cleanse`. Secret material is never logged or written to stdout/stderr.
5. **Forensic Transparency**: Forensic inspection modes distinguish between cryptographically verified RogueByte container signatures and heuristic statistical anomalies (LSB Shannon entropy and Chi-Square distribution).

### Memory Safety & Allocation Bounds

- Container headers explicitly validate lengths prior to allocation.
- Deserialization rejects allocations exceeding available buffer boundaries or platform safety limits.
- Atomic file writes prevent partial file truncation on unexpected termination.
