#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <filesystem>

namespace roguebyte {

enum class ExitCode : int {
    Success = 0,
    GeneralError = 1,
    InvalidArguments = 2,
    CoverError = 3,
    PayloadError = 4,
    InsufficientCapacity = 5,
    AuthenticationFailed = 6,
    IntegrityFailed = 7,
    SecurityViolation = 8
};

// Cryptographically secure memory clearing
void secure_zero(void* ptr, size_t len);

// Hex helpers
std::string to_hex(const uint8_t* data, size_t len);
std::vector<uint8_t> from_hex(const std::string& hex);

// Path traversal and safety defenses
// Returns true only if filename is safe (no .., no absolute separators, no control characters)
bool is_safe_filename(const std::string& name);

// Strips directory components and hazardous characters, returning clean basename
std::string sanitize_basename(const std::string& path);

// Binary file I/O
std::vector<uint8_t> read_file_binary(const std::filesystem::path& path);
bool write_file_atomic(const std::filesystem::path& target_path, const uint8_t* data, size_t len);

// File metadata
uint64_t get_file_mtime(const std::filesystem::path& path);
uint64_t get_file_size(const std::filesystem::path& path);

// String helpers
std::string format_bytes(uint64_t bytes);

} // namespace roguebyte
