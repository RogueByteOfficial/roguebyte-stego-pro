#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <array>
#include <filesystem>

namespace roguebyte {

constexpr size_t CRYPTO_SALT_LEN = 32;
constexpr size_t CRYPTO_NONCE_LEN = 12;
constexpr size_t CRYPTO_TAG_LEN = 16;
constexpr size_t CRYPTO_KEY_LEN = 32;
constexpr size_t SHA256_DIGEST_LEN = 32;
constexpr uint32_t DEFAULT_KDF_ITERATIONS = 100000;

struct EncryptionResult {
    bool success{false};
    std::vector<uint8_t> ciphertext;
    std::array<uint8_t, CRYPTO_SALT_LEN> salt{};
    std::array<uint8_t, CRYPTO_NONCE_LEN> nonce{};
    std::array<uint8_t, CRYPTO_TAG_LEN> tag{};
    uint32_t iterations{DEFAULT_KDF_ITERATIONS};
    std::string error_message;
};

struct DecryptionResult {
    bool success{false};
    std::vector<uint8_t> plaintext;
    std::string error_message;
};

// SHA-256
std::array<uint8_t, SHA256_DIGEST_LEN> sha256_digest(const uint8_t* data, size_t len);
std::string sha256_hex(const uint8_t* data, size_t len);
std::string sha256_file_hex(const std::filesystem::path& path);

// Cryptographically secure random bytes
bool get_random_bytes(uint8_t* out, size_t len);

// PBKDF2-HMAC-SHA256
bool derive_key_pbkdf2(
    const std::string& password,
    const uint8_t* salt, size_t salt_len,
    uint32_t iterations,
    uint8_t* out_key, size_t key_len
);

// Low-level AES-256-GCM
bool aes_256_gcm_encrypt(
    const uint8_t* plaintext, size_t plaintext_len,
    const uint8_t* key,
    const uint8_t* nonce, size_t nonce_len,
    const uint8_t* aad, size_t aad_len,
    std::vector<uint8_t>& out_ciphertext,
    uint8_t* out_tag, size_t tag_len
);

bool aes_256_gcm_decrypt(
    const uint8_t* ciphertext, size_t ciphertext_len,
    const uint8_t* key,
    const uint8_t* nonce, size_t nonce_len,
    const uint8_t* tag, size_t tag_len,
    const uint8_t* aad, size_t aad_len,
    std::vector<uint8_t>& out_plaintext
);

// High-level envelope encryption
EncryptionResult encrypt_buffer(
    const uint8_t* plaintext, size_t len,
    const std::string& password,
    uint32_t iterations = DEFAULT_KDF_ITERATIONS
);

DecryptionResult decrypt_buffer(
    const uint8_t* ciphertext, size_t len,
    const std::string& password,
    const uint8_t* salt, size_t salt_len,
    uint32_t iterations,
    const uint8_t* nonce, size_t nonce_len,
    const uint8_t* tag, size_t tag_len
);

} // namespace roguebyte
