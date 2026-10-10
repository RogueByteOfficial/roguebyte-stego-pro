#include "roguebyte_stego/container.hpp"
#include "roguebyte_stego/common.hpp"
#include <cassert>
#include <iostream>

using namespace roguebyte;

void test_container_serialization() {
    std::string data = "Container payload content test 1234567890";
    std::vector<uint8_t> payload(data.begin(), data.end());

    std::string err;
    auto container = build_container(payload, false, "", false, 0, &err);
    assert(!container.empty());
    assert(has_container_magic(container.data(), container.size()));

    auto parsed = parse_container(container.data(), container.size());
    assert(parsed.success);
    assert(!parsed.header.is_encrypted());
    assert(parsed.header.payload_size == payload.size());
    assert(parsed.payload == payload);

    std::vector<uint8_t> extracted;
    std::string ext_err;
    assert(extract_container_payload(parsed, "", extracted, ext_err));
    assert(extracted == payload);

    std::cout << "[PASS] test_container_serialization (plaintext)\n";
}

void test_container_encryption() {
    std::string data = "Encrypted container payload with high security credentials";
    std::vector<uint8_t> payload(data.begin(), data.end());
    std::string pass = "SecureKey#123";

    std::string err;
    auto container = build_container(payload, true, pass, false, 5000, &err);
    assert(!container.empty());

    auto parsed = parse_container(container.data(), container.size());
    assert(parsed.success);
    assert(parsed.header.is_encrypted());

    // Extract with correct password
    std::vector<uint8_t> extracted;
    std::string ext_err;
    assert(extract_container_payload(parsed, pass, extracted, ext_err));
    assert(extracted == payload);

    // Extract with wrong password fails
    std::vector<uint8_t> bad_extracted;
    std::string bad_err;
    assert(!extract_container_payload(parsed, "WrongPass!", bad_extracted, bad_err));

    std::cout << "[PASS] test_container_encryption (encrypted)\n";
}

void test_malformed_container() {
    // Truncated header
    uint8_t short_buf[10] = {'R', 'B', 'S', '1', 1, 0, 0, 0, 0, 0};
    std::string err;
    auto opt = parse_container_header(short_buf, sizeof(short_buf), err);
    assert(!opt.has_value());

    // Corrupted magic
    uint8_t bad_magic[CONTAINER_HEADER_SIZE] = {0};
    bad_magic[0] = 'X'; bad_magic[1] = 'Y'; bad_magic[2] = 'Z'; bad_magic[3] = '1';
    auto opt2 = parse_container_header(bad_magic, sizeof(bad_magic), err);
    assert(!opt2.has_value());

    std::cout << "[PASS] test_malformed_container\n";
}

int main() {
    std::cout << "=== Running Unit Tests: Container ===\n";
    test_container_serialization();
    test_container_encryption();
    test_malformed_container();
    std::cout << "All container unit tests passed!\n";
    return 0;
}
