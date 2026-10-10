#include "roguebyte_stego/container.hpp"
#include "roguebyte_stego/common.hpp"

#include <cstring>
#include <algorithm>

namespace roguebyte {

namespace {

inline uint16_t read_le16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

inline void write_le16(uint8_t* p, uint16_t val) {
    p[0] = static_cast<uint8_t>(val & 0xFF);
    p[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
}

inline uint32_t read_le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

inline void write_le32(uint8_t* p, uint32_t val) {
    p[0] = static_cast<uint8_t>(val & 0xFF);
    p[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((val >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((val >> 24) & 0xFF);
}

inline uint64_t read_le64(const uint8_t* p) {
    uint64_t val = 0;
    for (size_t i = 0; i < 8; ++i) {
        val |= (static_cast<uint64_t>(p[i]) << (i * 8));
    }
    return val;
}

inline void write_le64(uint8_t* p, uint64_t val) {
    for (size_t i = 0; i < 8; ++i) {
        p[i] = static_cast<uint8_t>((val >> (i * 8)) & 0xFF);
    }
}

} // anonymous namespace

bool has_container_magic(const uint8_t* data, size_t len) {
    if (!data || len < sizeof(CONTAINER_MAGIC)) return false;
    return std::memcmp(data, CONTAINER_MAGIC, sizeof(CONTAINER_MAGIC)) == 0;
}

std::vector<uint8_t> serialize_container(
    const ContainerHeader& header,
    const uint8_t* payload,
    size_t payload_len
) {
    std::vector<uint8_t> buffer(CONTAINER_HEADER_SIZE + payload_len);

    // Magic
    std::memcpy(buffer.data(), CONTAINER_MAGIC, 4);
    buffer[4] = header.version;
    buffer[5] = header.flags;
    write_le16(buffer.data() + 6, 0); // reserved
    write_le32(buffer.data() + 8, header.kdf_iterations);
    std::memcpy(buffer.data() + 12, header.salt.data(), CRYPTO_SALT_LEN);
    std::memcpy(buffer.data() + 44, header.nonce.data(), CRYPTO_NONCE_LEN);
    std::memcpy(buffer.data() + 56, header.tag.data(), CRYPTO_TAG_LEN);
    write_le64(buffer.data() + 72, payload_len);
    std::memcpy(buffer.data() + 80, header.payload_sha256.data(), SHA256_DIGEST_LEN);

    if (payload_len > 0 && payload != nullptr) {
        std::memcpy(buffer.data() + CONTAINER_HEADER_SIZE, payload, payload_len);
    }

    return buffer;
}

std::optional<ContainerHeader> parse_container_header(const uint8_t* data, size_t len, std::string& err) {
    if (!data || len < CONTAINER_HEADER_SIZE) {
        err = "Data buffer is too small to contain a RogueByte container header";
        return std::nullopt;
    }

    if (!has_container_magic(data, len)) {
        err = "Invalid container magic signature (expected 'RBS1')";
        return std::nullopt;
    }

    uint8_t version = data[4];
    if (version != CONTAINER_VERSION_1) {
        err = "Unsupported container version: " + std::to_string(version);
        return std::nullopt;
    }

    ContainerHeader hdr;
    hdr.version = version;
    hdr.flags = data[5];
    // reserved at data + 6
    hdr.kdf_iterations = read_le32(data + 8);
    std::memcpy(hdr.salt.data(), data + 12, CRYPTO_SALT_LEN);
    std::memcpy(hdr.nonce.data(), data + 44, CRYPTO_NONCE_LEN);
    std::memcpy(hdr.tag.data(), data + 56, CRYPTO_TAG_LEN);
    hdr.payload_size = read_le64(data + 72);
    std::memcpy(hdr.payload_sha256.data(), data + 80, SHA256_DIGEST_LEN);

    return hdr;
}

ContainerParseResult parse_container(const uint8_t* data, size_t len) {
    ContainerParseResult res;
    std::string err;
    auto hdr_opt = parse_container_header(data, len, err);
    if (!hdr_opt) {
        res.error_message = err;
        return res;
    }

    res.header = *hdr_opt;
    uint64_t available_payload = len - CONTAINER_HEADER_SIZE;
    if (res.header.payload_size > available_payload) {
        res.error_message = "Container payload is truncated (header specifies " +
                            std::to_string(res.header.payload_size) + " bytes, but only " +
                            std::to_string(available_payload) + " bytes available)";
        return res;
    }

    res.payload.resize(static_cast<size_t>(res.header.payload_size));
    if (res.header.payload_size > 0) {
        std::memcpy(res.payload.data(), data + CONTAINER_HEADER_SIZE, static_cast<size_t>(res.header.payload_size));
    }

    res.success = true;
    return res;
}

std::vector<uint8_t> build_container(
    const std::vector<uint8_t>& plaintext_payload,
    bool encrypt,
    const std::string& password,
    bool is_multi_file,
    uint32_t kdf_iterations,
    std::string* err_out
) {
    ContainerHeader hdr;
    hdr.version = CONTAINER_VERSION_1;
    hdr.flags = 0;
    if (is_multi_file) {
        hdr.flags |= FLAG_MULTI_FILE;
    }

    // Compute plaintext SHA-256 digest
    hdr.payload_sha256 = sha256_digest(plaintext_payload.data(), plaintext_payload.size());

    if (encrypt) {
        hdr.flags |= FLAG_ENCRYPTED;
        hdr.kdf_iterations = (kdf_iterations > 0) ? kdf_iterations : DEFAULT_KDF_ITERATIONS;

        auto enc_res = encrypt_buffer(plaintext_payload.data(), plaintext_payload.size(), password, hdr.kdf_iterations);
        if (!enc_res.success) {
            if (err_out) *err_out = enc_res.error_message;
            return {};
        }

        hdr.salt = enc_res.salt;
        hdr.nonce = enc_res.nonce;
        hdr.tag = enc_res.tag;
        hdr.payload_size = enc_res.ciphertext.size();

        return serialize_container(hdr, enc_res.ciphertext.data(), enc_res.ciphertext.size());
    } else {
        hdr.payload_size = plaintext_payload.size();
        return serialize_container(hdr, plaintext_payload.data(), plaintext_payload.size());
    }
}

bool extract_container_payload(
    const ContainerParseResult& parsed,
    const std::string& password,
    std::vector<uint8_t>& out_plaintext,
    std::string& err_out
) {
    if (!parsed.success) {
        err_out = "Cannot extract from invalid container: " + parsed.error_message;
        return false;
    }

    if (parsed.header.is_encrypted()) {
        if (password.empty()) {
            err_out = "Container is password-encrypted; password required";
            return false;
        }

        auto dec_res = decrypt_buffer(
            parsed.payload.data(), parsed.payload.size(),
            password,
            parsed.header.salt.data(), parsed.header.salt.size(),
            parsed.header.kdf_iterations,
            parsed.header.nonce.data(), parsed.header.nonce.size(),
            parsed.header.tag.data(), parsed.header.tag.size()
        );

        if (!dec_res.success) {
            err_out = dec_res.error_message;
            return false;
        }

        // Verify SHA-256 of decrypted payload against container header hash
        auto computed_hash = sha256_digest(dec_res.plaintext.data(), dec_res.plaintext.size());
        if (std::memcmp(computed_hash.data(), parsed.header.payload_sha256.data(), SHA256_DIGEST_LEN) != 0) {
            err_out = "Decrypted payload failed integrity verification (SHA-256 mismatch)";
            return false;
        }

        out_plaintext = std::move(dec_res.plaintext);
        return true;
    } else {
        // Plaintext container: verify SHA-256 integrity
        auto computed_hash = sha256_digest(parsed.payload.data(), parsed.payload.size());
        if (std::memcmp(computed_hash.data(), parsed.header.payload_sha256.data(), SHA256_DIGEST_LEN) != 0) {
            err_out = "Payload failed integrity verification (SHA-256 mismatch)";
            return false;
        }

        out_plaintext = parsed.payload;
        return true;
    }
}

} // namespace roguebyte
