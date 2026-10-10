#include "roguebyte_stego/image_backend.hpp"
#include "roguebyte_stego/crypto.hpp"
#include "roguebyte_stego/container.hpp"
#include "roguebyte_stego/common.hpp"
#include "lodepng/lodepng.h"

#include <cassert>
#include <iostream>
#include <filesystem>

using namespace roguebyte;

void test_capacity_rejection() {
    std::cout << "[*] Running Security Test: Capacity Overload Rejection...\n";

    // 10x10 PNG = 100 pixels * 3 / 8 = 37 bytes capacity
    uint32_t w = 10, h = 10;
    std::vector<uint8_t> pixels(w * h * 4, 128);
    std::vector<uint8_t> png_bytes;
    lodepng::encode(png_bytes, pixels, w, h);

    std::filesystem::path small_cover = "/tmp/sec_small.png";
    write_file_atomic(small_cover, png_bytes.data(), png_bytes.size());

    std::string err;
    auto backend = create_backend_for_file(small_cover, err);
    assert(backend != nullptr);

    // Header alone is 112 bytes, so even empty container won't fit in 37 bytes
    std::vector<uint8_t> dummy_container(150, 0xAA);
    bool res = backend->embed_container(dummy_container, err);
    assert(!res); // MUST BE REJECTED
    assert(err.find("exceeds") != std::string::npos);

    std::cout << "[PASS] Capacity overload cleanly rejected.\n";
}

void test_tampered_container_rejection() {
    std::cout << "[*] Running Security Test: Tampered Data Authentication Failure...\n";

    std::vector<uint8_t> secret = {'D', 'A', 'T', 'A'};
    std::string pass = "CorrectPass123";
    auto container = build_container(secret, true, pass, false, 5000);
    assert(!container.empty());

    // Tamper with ciphertext byte (past header)
    auto tampered = container;
    tampered[CONTAINER_HEADER_SIZE + 1] ^= 0xFF;

    auto parsed = parse_container(tampered.data(), tampered.size());
    assert(parsed.success);

    std::vector<uint8_t> decrypted;
    std::string err;
    bool success = extract_container_payload(parsed, pass, decrypted, err);
    assert(!success); // MUST FAIL AEAD verification!
    assert(err.find("failed") != std::string::npos);

    std::cout << "[PASS] Tampered ciphertext properly rejected by AEAD authentication.\n";
}

int main() {
    std::cout << "=== Running Integration Test: Security ===\n";
    test_capacity_rejection();
    test_tampered_container_rejection();
    return 0;
}
