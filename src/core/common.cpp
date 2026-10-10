#include "roguebyte_stego/common.hpp"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <system_error>
#include <algorithm>
#include <openssl/crypto.h>

namespace roguebyte {

void secure_zero(void* ptr, size_t len) {
    if (ptr && len > 0) {
        OPENSSL_cleanse(ptr, len);
    }
}

std::string to_hex(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

std::vector<uint8_t> from_hex(const std::string& hex) {
    std::vector<uint8_t> bytes;
    if (hex.length() % 2 != 0) return bytes;
    bytes.reserve(hex.length() / 2);
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte_str = hex.substr(i, 2);
        char* end = nullptr;
        unsigned long val = std::strtoul(byte_str.c_str(), &end, 16);
        if (end != byte_str.c_str() + 2) return {};
        bytes.push_back(static_cast<uint8_t>(val));
    }
    return bytes;
}

bool is_safe_filename(const std::string& name) {
    if (name.empty() || name == "." || name == "..") return false;
    if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) return false;
    if (name.find("..") != std::string::npos) return false;
    for (char c : name) {
        if (static_cast<unsigned char>(c) < 32 || c == 127) return false; // control characters
    }
    return true;
}

std::string sanitize_basename(const std::string& path_str) {
    std::filesystem::path p(path_str);
    std::string base = p.filename().string();
    if (base.empty()) {
        base = "extracted_payload.bin";
    }
    // Replace dangerous chars
    for (char& c : base) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        } else if (static_cast<unsigned char>(c) < 32 || c == 127) {
            c = '_';
        }
    }
    if (base == "." || base == "..") {
        base = "unnamed_payload.bin";
    }
    return base;
}

std::vector<uint8_t> read_file_binary(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }
    auto size = file.tellg();
    if (size <= 0) {
        return {};
    }
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    if (!file) {
        return {};
    }
    return buffer;
}

bool write_file_atomic(const std::filesystem::path& target_path, const uint8_t* data, size_t len) {
    auto dir = target_path.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir)) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) return false;
    }

    std::filesystem::path temp_path = target_path;
    temp_path += ".rbs_tmp_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

    {
        std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) return false;
        if (len > 0 && data != nullptr) {
            file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(len));
        }
        if (!file) {
            file.close();
            std::filesystem::remove(temp_path);
            return false;
        }
        file.flush();
    }

    std::error_code ec;
    std::filesystem::rename(temp_path, target_path, ec);
    if (ec) {
        // Fallback for cross-device moves or platforms where atomic rename fails over existing
        std::filesystem::remove(target_path, ec);
        std::filesystem::rename(temp_path, target_path, ec);
        if (ec) {
            std::filesystem::remove(temp_path, ec);
            return false;
        }
    }
    return true;
}

uint64_t get_file_mtime(const std::filesystem::path& path) {
    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count());
}

uint64_t get_file_size(const std::filesystem::path& path) {
    std::error_code ec;
    auto s = std::filesystem::file_size(path, ec);
    return ec ? 0 : s;
}

std::string format_bytes(uint64_t bytes) {
    const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    int unit_idx = 0;
    double count = static_cast<double>(bytes);
    while (count >= 1024.0 && unit_idx < 4) {
        count /= 1024.0;
        unit_idx++;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(unit_idx == 0 ? 0 : 2) << count << " " << units[unit_idx];
    return oss.str();
}

} // namespace roguebyte
