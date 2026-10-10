#include "roguebyte_stego/crypto.hpp"
#include "roguebyte_stego/common.hpp"
#include <cassert>
#include <iostream>
#include <cstring>

using namespace roguebyte;

void test_sha256() {
    // Known vector: SHA256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    std::string empty_hash = sha256_hex(nullptr, 0);
    assert(empty_hash == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // Known vector: SHA256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    std::string abc = "abc";
    std::string abc_hash = sha256_hex(reinterpret_cast<const uint8_t*>(abc.data()), abc.length());
    assert(abc_hash == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    std::cout << "[PASS] test_sha256\n";
}

void test_pbkdf2() {
    std::string pass = "RogueByteSecret2026!";
    uint8_t salt[CRYPTO_SALT_LEN] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                                     17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
    uint8_t key1[CRYPTO_KEY_LEN] = {0};
    uint8_t key2[CRYPTO_KEY_LEN] = {0};

    // Deterministic key generation
    assert(derive_key_pbkdf2(pass, salt, sizeof(salt), 1000, key1, sizeof(key1)));
    assert(derive_key_pbkdf2(pass, salt, sizeof(salt), 1000, key2, sizeof(key2)));
    assert(std::memcmp(key1, key2, sizeof(key1)) == 0);

    // Different salt yields different key
    uint8_t salt_diff[CRYPTO_SALT_LEN] = {0};
    uint8_t key3[CRYPTO_KEY_LEN] = {0};
    assert(derive_key_pbkdf2(pass, salt_diff, sizeof(salt_diff), 1000, key3, sizeof(key3)));
    assert(std::memcmp(key1, key3, sizeof(key1)) != 0);

    std::cout << "[PASS] test_pbkdf2\n";
}

void test_aes_gcm_roundtrip() {
    std::string secret_msg = "Top-secret payload for digital forensic transmission.";
    std::string password = "StrongOperationalPassword#99!";

    auto enc = encrypt_buffer(
        reinterpret_cast<const uint8_t*>(secret_msg.data()),
        secret_msg.length(),
        password,
        10000
    );
    assert(enc.success);
    assert(!enc.ciphertext.empty());

    // Correct decryption
    auto dec = decrypt_buffer(
        enc.ciphertext.data(), enc.ciphertext.size(),
        password,
        enc.salt.data(), enc.salt.size(),
        enc.iterations,
        enc.nonce.data(), enc.nonce.size(),
        enc.tag.data(), enc.tag.size()
    );
    assert(dec.success);
    std::string recovered(reinterpret_cast<char*>(dec.plaintext.data()), dec.plaintext.size());
    assert(recovered == secret_msg);

    // Wrong password fails
    auto dec_wrong_pass = decrypt_buffer(
        enc.ciphertext.data(), enc.ciphertext.size(),
        "WrongPassword!",
        enc.salt.data(), enc.salt.size(),
        enc.iterations,
        enc.nonce.data(), enc.nonce.size(),
        enc.tag.data(), enc.tag.size()
    );
    assert(!dec_wrong_pass.success);

    // Tampered ciphertext fails
    auto tampered = enc.ciphertext;
    tampered[0] ^= 0x01;
    auto dec_tampered = decrypt_buffer(
        tampered.data(), tampered.size(),
        password,
        enc.salt.data(), enc.salt.size(),
        enc.iterations,
        enc.nonce.data(), enc.nonce.size(),
        enc.tag.data(), enc.tag.size()
    );
    assert(!dec_tampered.success);

    std::cout << "[PASS] test_aes_gcm_roundtrip\n";
}

int main() {
    std::cout << "=== Running Unit Tests: Crypto ===\n";
    test_sha256();
    test_pbkdf2();
    test_aes_gcm_roundtrip();
    std::cout << "All crypto unit tests passed!\n";
    return 0;
}
