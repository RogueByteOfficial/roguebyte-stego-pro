#include "roguebyte_stego/crypto.hpp"
#include "roguebyte_stego/common.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <fstream>
#include <cstring>

namespace roguebyte {

std::array<uint8_t, SHA256_DIGEST_LEN> sha256_digest(const uint8_t* data, size_t len) {
    std::array<uint8_t, SHA256_DIGEST_LEN> hash{};
    unsigned int out_len = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx) {
        if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1) {
            if (len == 0 || EVP_DigestUpdate(ctx, data, len) == 1) {
                EVP_DigestFinal_ex(ctx, hash.data(), &out_len);
            }
        }
        EVP_MD_CTX_free(ctx);
    }
    return hash;
}

std::string sha256_hex(const uint8_t* data, size_t len) {
    auto digest = sha256_digest(data, len);
    return to_hex(digest.data(), digest.size());
}

std::string sha256_file_hex(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    std::vector<char> buffer(64 * 1024);
    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
        if (EVP_DigestUpdate(ctx, buffer.data(), static_cast<size_t>(file.gcount())) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }
    }

    std::array<uint8_t, SHA256_DIGEST_LEN> digest{};
    unsigned int out_len = 0;
    EVP_DigestFinal_ex(ctx, digest.data(), &out_len);
    EVP_MD_CTX_free(ctx);

    return to_hex(digest.data(), digest.size());
}

bool get_random_bytes(uint8_t* out, size_t len) {
    if (!out || len == 0) return false;
    return RAND_bytes(out, static_cast<int>(len)) == 1;
}

bool derive_key_pbkdf2(
    const std::string& password,
    const uint8_t* salt, size_t salt_len,
    uint32_t iterations,
    uint8_t* out_key, size_t key_len
) {
    if (password.empty() || !salt || salt_len == 0 || !out_key || key_len == 0 || iterations == 0) {
        return false;
    }

    int rc = PKCS5_PBKDF2_HMAC(
        password.c_str(),
        static_cast<int>(password.length()),
        salt,
        static_cast<int>(salt_len),
        static_cast<int>(iterations),
        EVP_sha256(),
        static_cast<int>(key_len),
        out_key
    );

    return rc == 1;
}

bool aes_256_gcm_encrypt(
    const uint8_t* plaintext, size_t plaintext_len,
    const uint8_t* key,
    const uint8_t* nonce, size_t nonce_len,
    const uint8_t* aad, size_t aad_len,
    std::vector<uint8_t>& out_ciphertext,
    uint8_t* out_tag, size_t tag_len
) {
    if (!key || !nonce || nonce_len != CRYPTO_NONCE_LEN || !out_tag || tag_len != CRYPTO_TAG_LEN) {
        return false;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    bool ok = false;
    do {
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(nonce_len), nullptr) != 1) break;
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, nonce) != 1) break;

        // Additional Authenticated Data
        if (aad && aad_len > 0) {
            int aad_out = 0;
            if (EVP_EncryptUpdate(ctx, nullptr, &aad_out, aad, static_cast<int>(aad_len)) != 1) break;
        }

        out_ciphertext.resize(plaintext_len);
        int out_len = 0;
        if (plaintext_len > 0 && plaintext != nullptr) {
            if (EVP_EncryptUpdate(ctx, out_ciphertext.data(), &out_len, plaintext, static_cast<int>(plaintext_len)) != 1) break;
        }

        int final_len = 0;
        if (EVP_EncryptFinal_ex(ctx, out_ciphertext.data() + out_len, &final_len) != 1) break;

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(tag_len), out_tag) != 1) break;

        ok = true;
    } while (false);

    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

bool aes_256_gcm_decrypt(
    const uint8_t* ciphertext, size_t ciphertext_len,
    const uint8_t* key,
    const uint8_t* nonce, size_t nonce_len,
    const uint8_t* tag, size_t tag_len,
    const uint8_t* aad, size_t aad_len,
    std::vector<uint8_t>& out_plaintext
) {
    if (!key || !nonce || nonce_len != CRYPTO_NONCE_LEN || !tag || tag_len != CRYPTO_TAG_LEN) {
        return false;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    bool ok = false;
    do {
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(nonce_len), nullptr) != 1) break;
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key, nonce) != 1) break;

        if (aad && aad_len > 0) {
            int aad_out = 0;
            if (EVP_DecryptUpdate(ctx, nullptr, &aad_out, aad, static_cast<int>(aad_len)) != 1) break;
        }

        out_plaintext.resize(ciphertext_len);
        int out_len = 0;
        if (ciphertext_len > 0 && ciphertext != nullptr) {
            if (EVP_DecryptUpdate(ctx, out_plaintext.data(), &out_len, ciphertext, static_cast<int>(ciphertext_len)) != 1) break;
        }

        // Set expected verification tag before final verification
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(tag_len), const_cast<uint8_t*>(tag)) != 1) break;

        int final_len = 0;
        int ret = EVP_DecryptFinal_ex(ctx, out_plaintext.data() + out_len, &final_len);
        if (ret > 0) {
            ok = true;
        }
    } while (false);

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) {
        out_plaintext.clear();
    }
    return ok;
}

EncryptionResult encrypt_buffer(
    const uint8_t* plaintext, size_t len,
    const std::string& password,
    uint32_t iterations
) {
    EncryptionResult res;
    res.iterations = (iterations > 0) ? iterations : DEFAULT_KDF_ITERATIONS;

    if (password.empty()) {
        res.error_message = "Password cannot be empty for authenticated encryption";
        return res;
    }

    if (!get_random_bytes(res.salt.data(), res.salt.size())) {
        res.error_message = "Cryptographic RNG failure generating salt";
        return res;
    }

    if (!get_random_bytes(res.nonce.data(), res.nonce.size())) {
        res.error_message = "Cryptographic RNG failure generating nonce";
        return res;
    }

    std::array<uint8_t, CRYPTO_KEY_LEN> key{};
    if (!derive_key_pbkdf2(password, res.salt.data(), res.salt.size(), res.iterations, key.data(), key.size())) {
        res.error_message = "Key derivation failed";
        return res;
    }

    if (!aes_256_gcm_encrypt(plaintext, len, key.data(), res.nonce.data(), res.nonce.size(), nullptr, 0,
                             res.ciphertext, res.tag.data(), res.tag.size())) {
        secure_zero(key.data(), key.size());
        res.error_message = "AES-256-GCM encryption failed";
        return res;
    }

    secure_zero(key.data(), key.size());
    res.success = true;
    return res;
}

DecryptionResult decrypt_buffer(
    const uint8_t* ciphertext, size_t len,
    const std::string& password,
    const uint8_t* salt, size_t salt_len,
    uint32_t iterations,
    const uint8_t* nonce, size_t nonce_len,
    const uint8_t* tag, size_t tag_len
) {
    DecryptionResult res;
    if (password.empty()) {
        res.error_message = "Password cannot be empty for decryption";
        return res;
    }
    if (!salt || salt_len != CRYPTO_SALT_LEN) {
        res.error_message = "Invalid salt length";
        return res;
    }
    if (!nonce || nonce_len != CRYPTO_NONCE_LEN) {
        res.error_message = "Invalid nonce length";
        return res;
    }
    if (!tag || tag_len != CRYPTO_TAG_LEN) {
        res.error_message = "Invalid authentication tag length";
        return res;
    }

    std::array<uint8_t, CRYPTO_KEY_LEN> key{};
    if (!derive_key_pbkdf2(password, salt, salt_len, iterations, key.data(), key.size())) {
        res.error_message = "Key derivation failed during decryption";
        return res;
    }

    if (!aes_256_gcm_decrypt(ciphertext, len, key.data(), nonce, nonce_len, tag, tag_len, nullptr, 0, res.plaintext)) {
        secure_zero(key.data(), key.size());
        res.error_message = "Authentication tag verification failed (incorrect password or corrupted/tampered data)";
        return res;
    }

    secure_zero(key.data(), key.size());
    res.success = true;
    return res;
}

} // namespace roguebyte
