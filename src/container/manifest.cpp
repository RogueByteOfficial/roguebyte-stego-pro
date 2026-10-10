#include "roguebyte_stego/manifest.hpp"
#include "roguebyte_stego/common.hpp"

#include <cstring>
#include <system_error>

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

bool has_manifest_magic(const uint8_t* data, size_t len) {
    if (!data || len < sizeof(MANIFEST_MAGIC)) return false;
    return std::memcmp(data, MANIFEST_MAGIC, sizeof(MANIFEST_MAGIC)) == 0;
}

bool build_manifest_from_files(
    const std::vector<std::filesystem::path>& file_paths,
    Manifest& out_manifest,
    std::string& err_out
) {
    out_manifest.entries.clear();
    if (file_paths.empty()) {
        err_out = "No input files provided to manifest";
        return false;
    }

    for (const auto& path : file_paths) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec)) {
            err_out = "Input file does not exist or is not a regular file: " + path.string();
            return false;
        }

        ManifestEntry entry;
        entry.original_filename = path.filename().string();
        entry.safe_filename = sanitize_basename(entry.original_filename);
        entry.mtime_sec = get_file_mtime(path);
        entry.data = read_file_binary(path);
        entry.file_size = entry.data.size();
        entry.sha256 = sha256_digest(entry.data.data(), entry.data.size());

        out_manifest.entries.push_back(std::move(entry));
    }

    return true;
}

std::vector<uint8_t> serialize_manifest(const Manifest& manifest) {
    // Calculate total size
    size_t total_size = MANIFEST_HEADER_SIZE;
    for (const auto& entry : manifest.entries) {
        std::string fname = entry.safe_filename.empty() ? entry.original_filename : entry.safe_filename;
        if (fname.length() > MAX_FILENAME_LENGTH) {
            fname = fname.substr(0, MAX_FILENAME_LENGTH);
        }
        total_size += 2 + fname.length() + 8 + 8 + SHA256_DIGEST_LEN + entry.data.size();
    }

    std::vector<uint8_t> buffer(total_size);
    std::memcpy(buffer.data(), MANIFEST_MAGIC, 4);
    write_le32(buffer.data() + 4, static_cast<uint32_t>(manifest.entries.size()));

    size_t offset = MANIFEST_HEADER_SIZE;
    for (const auto& entry : manifest.entries) {
        std::string fname = entry.safe_filename.empty() ? entry.original_filename : entry.safe_filename;
        if (fname.length() > MAX_FILENAME_LENGTH) {
            fname = fname.substr(0, MAX_FILENAME_LENGTH);
        }
        uint16_t name_len = static_cast<uint16_t>(fname.length());
        write_le16(buffer.data() + offset, name_len);
        offset += 2;

        std::memcpy(buffer.data() + offset, fname.data(), name_len);
        offset += name_len;

        write_le64(buffer.data() + offset, entry.data.size());
        offset += 8;

        write_le64(buffer.data() + offset, entry.mtime_sec);
        offset += 8;

        std::memcpy(buffer.data() + offset, entry.sha256.data(), SHA256_DIGEST_LEN);
        offset += SHA256_DIGEST_LEN;

        if (!entry.data.empty()) {
            std::memcpy(buffer.data() + offset, entry.data.data(), entry.data.size());
            offset += entry.data.size();
        }
    }

    return buffer;
}

bool deserialize_manifest(
    const uint8_t* data,
    size_t len,
    Manifest& out_manifest,
    std::string& err_out
) {
    out_manifest.entries.clear();
    if (!data || len < MANIFEST_HEADER_SIZE) {
        err_out = "Data too small for manifest header";
        return false;
    }

    if (!has_manifest_magic(data, len)) {
        err_out = "Invalid manifest magic (expected 'RBM1')";
        return false;
    }

    uint32_t count = read_le32(data + 4);
    if (count > MAX_MANIFEST_ENTRIES) {
        err_out = "Manifest entry count exceeds safety threshold: " + std::to_string(count);
        return false;
    }

    size_t offset = MANIFEST_HEADER_SIZE;
    for (uint32_t i = 0; i < count; ++i) {
        if (offset + 2 > len) {
            err_out = "Truncated manifest while reading filename length at entry " + std::to_string(i);
            return false;
        }

        uint16_t name_len = read_le16(data + offset);
        offset += 2;

        if (name_len > MAX_FILENAME_LENGTH || offset + name_len > len) {
            err_out = "Invalid or truncated filename length at entry " + std::to_string(i);
            return false;
        }

        std::string raw_name(reinterpret_cast<const char*>(data + offset), name_len);
        offset += name_len;

        if (offset + 8 + 8 + SHA256_DIGEST_LEN > len) {
            err_out = "Truncated metadata for manifest entry " + std::to_string(i);
            return false;
        }

        uint64_t file_size = read_le64(data + offset);
        offset += 8;

        uint64_t mtime_sec = read_le64(data + offset);
        offset += 8;

        std::array<uint8_t, SHA256_DIGEST_LEN> sha256_hash{};
        std::memcpy(sha256_hash.data(), data + offset, SHA256_DIGEST_LEN);
        offset += SHA256_DIGEST_LEN;

        if (offset + file_size > len) {
            err_out = "Truncated payload for file " + raw_name + " in manifest";
            return false;
        }

        ManifestEntry entry;
        entry.original_filename = raw_name;
        entry.safe_filename = sanitize_basename(raw_name);
        entry.file_size = file_size;
        entry.mtime_sec = mtime_sec;
        entry.sha256 = sha256_hash;

        entry.data.resize(static_cast<size_t>(file_size));
        if (file_size > 0) {
            std::memcpy(entry.data.data(), data + offset, static_cast<size_t>(file_size));
            offset += static_cast<size_t>(file_size);
        }

        // Verify SHA-256 integrity
        auto check_hash = sha256_digest(entry.data.data(), entry.data.size());
        if (std::memcmp(check_hash.data(), entry.sha256.data(), SHA256_DIGEST_LEN) != 0) {
            err_out = "Integrity check failed for manifest entry: " + entry.safe_filename;
            return false;
        }

        out_manifest.entries.push_back(std::move(entry));
    }

    return true;
}

bool unpack_manifest(
    const Manifest& manifest,
    const std::filesystem::path& destination_dir,
    std::vector<std::string>& written_files,
    std::string& err_out
) {
    written_files.clear();
    std::error_code ec;

    if (!std::filesystem::exists(destination_dir, ec)) {
        if (!std::filesystem::create_directories(destination_dir, ec)) {
            err_out = "Failed to create destination directory: " + destination_dir.string();
            return false;
        }
    }

    // Resolve canonical path of destination directory to defend against directory escape
    std::filesystem::path canonical_dest = std::filesystem::weakly_canonical(destination_dir, ec);
    if (ec) {
        canonical_dest = destination_dir;
    }

    for (const auto& entry : manifest.entries) {
        std::string safe_name = sanitize_basename(entry.safe_filename.empty() ? entry.original_filename : entry.safe_filename);
        std::filesystem::path target = canonical_dest / safe_name;

        // Path traversal defense: confirm target path starts with destination directory
        std::filesystem::path canonical_target = std::filesystem::weakly_canonical(target, ec);
        auto dest_str = canonical_dest.string();
        auto target_str = canonical_target.string();
        if (target_str.find(dest_str) != 0) {
            err_out = "Security violation: path traversal detected for file " + safe_name;
            return false;
        }

        if (!write_file_atomic(target, entry.data.data(), entry.data.size())) {
            err_out = "Failed to atomically write extracted file: " + target.string();
            return false;
        }

        written_files.push_back(target.string());
    }

    return true;
}

} // namespace roguebyte
