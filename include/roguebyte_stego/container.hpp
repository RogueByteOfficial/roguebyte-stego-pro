#pragma once

#include "roguebyte_stego/crypto.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <array>
#include <optional>

namespace roguebyte {

constexpr uint8_t CONTAINER_MAGIC[4] = {'R', 'B', 'S', '1'};
constexpr uint8_t CONTAINER_VERSION_1 = 0x01;

constexpr uint8_t FLAG_ENCRYPTED   = 0x01;
constexpr uint8_t FLAG_COMPRESSED  = 0x02;
constexpr uint8_t FLAG_MULTI_FILE  = 0x04;

constexpr size_t CONTAINER_HEADER_SIZE = 112;

#pragma pack(push, 1)
struct ContainerHeaderRaw {
    uint8_t magic[4];          // 'R', 'B', 'S', '1'
    uint8_t version;           // 0x01
    uint8_t flags;             // Bitflags: encrypted, compressed, multi-file
    uint16_t reserved;         // Reserved, must be 0
    uint32_t kdf_iterations;   // PBKDF2 iterations
    uint8_t salt[CRYPTO_SALT_LEN];    // 32 bytes
    uint8_t nonce[CRYPTO_NONCE_LEN];  // 12 bytes
    uint8_t tag[CRYPTO_TAG_LEN];      // 16 bytes
    uint64_t payload_size;     // Size of payload (ciphertext or plaintext)
    uint8_t payload_sha256[SHA256_DIGEST_LEN]; // 32 bytes
};
#pragma pack(pop)

static_assert(sizeof(ContainerHeaderRaw) == CONTAINER_HEADER_SIZE, "ContainerHeaderRaw must be exactly 112 bytes");

struct ContainerHeader {
    uint8_t version{CONTAINER_VERSION_1};
    uint8_t flags{0};
    uint32_t kdf_iterations{DEFAULT_KDF_ITERATIONS};
    std::array<uint8_t, CRYPTO_SALT_LEN> salt{};
    std::array<uint8_t, CRYPTO_NONCE_LEN> nonce{};
    std::array<uint8_t, CRYPTO_TAG_LEN> tag{};
    uint64_t payload_size{0};
    std::array<uint8_t, SHA256_DIGEST_LEN> payload_sha256{};

    bool is_encrypted() const { return (flags & FLAG_ENCRYPTED) != 0; }
    bool is_multi_file() const { return (flags & FLAG_MULTI_FILE) != 0; }
};

struct ContainerParseResult {
    bool success{false};
    ContainerHeader header;
    std::vector<uint8_t> payload;
    std::string error_message;
};

// Check if a byte buffer starts with the RBS1 magic
bool has_container_magic(const uint8_t* data, size_t len);

// Serialize a container into raw binary bytes
std::vector<uint8_t> serialize_container(
    const ContainerHeader& header,
    const uint8_t* payload,
    size_t payload_len
);

// Safely parse container header from data
std::optional<ContainerHeader> parse_container_header(const uint8_t* data, size_t len, std::string& err);

// Safely parse entire container (header + bounded payload)
ContainerParseResult parse_container(const uint8_t* data, size_t len);

// Create container with payload (optionally encrypted)
std::vector<uint8_t> build_container(
    const std::vector<uint8_t>& plaintext_payload,
    bool encrypt,
    const std::string& password,
    bool is_multi_file,
    uint32_t kdf_iterations = DEFAULT_KDF_ITERATIONS,
    std::string* err_out = nullptr
);

// Unpack payload from container (decrypts if encrypted)
bool extract_container_payload(
    const ContainerParseResult& parsed,
    const std::string& password,
    std::vector<uint8_t>& out_plaintext,
    std::string& err_out
);

} // namespace roguebyte
