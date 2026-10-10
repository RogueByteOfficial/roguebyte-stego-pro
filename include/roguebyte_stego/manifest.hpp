#pragma once

#include "roguebyte_stego/crypto.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <array>
#include <filesystem>

namespace roguebyte {

constexpr uint8_t MANIFEST_MAGIC[4] = {'R', 'B', 'M', '1'};
constexpr size_t MANIFEST_HEADER_SIZE = 8; // 'RBM1' + uint32_t count
constexpr size_t MAX_MANIFEST_ENTRIES = 10000;
constexpr size_t MAX_FILENAME_LENGTH = 255;

struct ManifestEntry {
    std::string original_filename;
    std::string safe_filename;
    uint64_t file_size{0};
    uint64_t mtime_sec{0};
    std::array<uint8_t, SHA256_DIGEST_LEN> sha256{};
    std::vector<uint8_t> data;
};

struct Manifest {
    std::vector<ManifestEntry> entries;
};

// Check if a byte buffer starts with RBM1 magic
bool has_manifest_magic(const uint8_t* data, size_t len);

// Create manifest from a list of disk file paths
bool build_manifest_from_files(
    const std::vector<std::filesystem::path>& file_paths,
    Manifest& out_manifest,
    std::string& err_out
);

// Serialize manifest to bytes
std::vector<uint8_t> serialize_manifest(const Manifest& manifest);

// Parse manifest from bytes (with strict length and path-traversal validation)
bool deserialize_manifest(
    const uint8_t* data,
    size_t len,
    Manifest& out_manifest,
    std::string& err_out
);

// Unpack manifest files into target directory safely (atomic writes, no traversal, verify each SHA-256)
bool unpack_manifest(
    const Manifest& manifest,
    const std::filesystem::path& destination_dir,
    std::vector<std::string>& written_files,
    std::string& err_out
);

} // namespace roguebyte
