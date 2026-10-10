# Forensic Methodology & Limitations

**Author**: SidoDev-Dz | **Organization**: RogueByte

## Steganalysis Capabilities

The inspection engine provides multi-layered digital forensic analysis:

1. **Structural File Verification**:
   - Parses native magic signatures (`PNG`, `BMP`, `JPEG`).
   - Validates header segment dimensions, row padding strides, and stream completeness.

2. **Bitplane Shannon Entropy Analysis**:
   - Computes Shannon entropy $H = -\sum p_i \log_2 p_i$ across all 8 bit planes for each color channel.
   - Natural uncompressed photographic images display lower entropy on least significant bits compared to higher bits.
   - High-density pseudo-random encrypted steganography drives LSB entropy toward theoretical maximum ($H \approx 1.00000$).

3. **Pairs of Values (PoV) Chi-Square Distribution**:
   - Assesses pairing parity across adjacent pixel values $(2k, 2k+1)$.
   - Equalized frequencies indicate statistical anomalies caused by LSB substitution.

4. **Signature Verification**:
   - Scans image bitstream and marker tables for RogueByte `RBS1` container headers.
   - Extracts non-confidential container metadata: version, flags, payload length, KDF iterations, salt, nonce, tag, and payload SHA-256 without needing the decryption password.

---

## Forensic Limitations

- **Clean Image Verdict**: RogueByte Stego Pro explicitly warns that absence of a known signature or statistical anomaly does **not** prove an image is clean. Modern adaptive steganography schemes (e.g. HUGO, WOW, S-UNIWARD) or low-capacity embeddings cannot be ruled out by simple signature matching.
- **Textured Cover Images**: High-noise cover photographs (e.g., foliage, sand, film grain) naturally possess elevated LSB entropy, which can produce heuristic false positives without container confirmation.
