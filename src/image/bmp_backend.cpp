#include "roguebyte_stego/bmp_backend.hpp"
#include "roguebyte_stego/common.hpp"

#include <cstring>
#include <cmath>
#include <iostream>

namespace roguebyte {

namespace {

inline uint16_t read_le16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

inline uint32_t read_le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

inline int32_t read_le32_signed(const uint8_t* p) {
    return static_cast<int32_t>(read_le32(p));
}

} // anonymous namespace

BmpBackend::BmpBackend() = default;

bool BmpBackend::load_file(const std::filesystem::path& path, std::string& err_out) {
    m_raw_file = read_file_binary(path);
    if (m_raw_file.size() < 54) {
        err_out = "File too small to be a valid BMP image";
        return false;
    }

    const uint8_t* data = m_raw_file.data();
    if (data[0] != 'B' || data[1] != 'M') {
        err_out = "Invalid BMP signature (expected 'BM')";
        return false;
    }

    m_pixel_offset = read_le32(data + 10);
    uint32_t dib_header_size = read_le32(data + 14);
    if (dib_header_size < 40) {
        err_out = "Unsupported old BMP DIB header size: " + std::to_string(dib_header_size);
        return false;
    }

    int32_t width = read_le32_signed(data + 18);
    int32_t height = read_le32_signed(data + 22);
    if (width <= 0 || height == 0) {
        err_out = "Invalid BMP dimensions";
        return false;
    }

    m_top_down = (height < 0);
    uint32_t u_height = static_cast<uint32_t>(std::abs(height));
    uint32_t u_width = static_cast<uint32_t>(width);

    uint16_t planes = read_le16(data + 26);
    if (planes != 1) {
        err_out = "Unsupported BMP planes count: " + std::to_string(planes);
        return false;
    }

    m_bpp = read_le16(data + 28);
    if (m_bpp != 24 && m_bpp != 32) {
        err_out = "Unsupported BMP color depth: " + std::to_string(m_bpp) + " bpp (expected 24 or 32 bpp uncompressed)";
        return false;
    }

    uint32_t compression = read_le32(data + 30);
    if (compression != 0 && compression != 3) { // 0 = BI_RGB, 3 = BI_BITFIELDS
        err_out = "Compressed BMP formats are not supported for lossless steganography";
        return false;
    }

    // Row stride: rows are padded to multiple of 4 bytes
    m_row_stride = ((u_width * (m_bpp / 8) + 3) / 4) * 4;

    size_t expected_pixel_bytes = static_cast<size_t>(m_row_stride) * u_height;
    if (m_pixel_offset + expected_pixel_bytes > m_raw_file.size()) {
        err_out = "Truncated BMP file data";
        return false;
    }

    m_dims.width = u_width;
    m_dims.height = u_height;
    m_dims.channels = 3; // RGB normalized
    m_dims.bit_depth = 8;

    // Build normalized RGB flat buffer for analysis
    m_rgb_pixels.resize(static_cast<size_t>(u_width) * u_height * 3);
    size_t bytes_per_pixel = m_bpp / 8;

    for (uint32_t y = 0; y < u_height; ++y) {
        uint32_t src_y = m_top_down ? y : (u_height - 1 - y);
        const uint8_t* row_src = data + m_pixel_offset + src_y * m_row_stride;
        for (uint32_t x = 0; x < u_width; ++x) {
            size_t src_idx = x * bytes_per_pixel;
            size_t dst_idx = (y * u_width + x) * 3;
            // BMP stores BGR(A)
            m_rgb_pixels[dst_idx + 0] = row_src[src_idx + 2]; // R
            m_rgb_pixels[dst_idx + 1] = row_src[src_idx + 1]; // G
            m_rgb_pixels[dst_idx + 2] = row_src[src_idx + 0]; // B
        }
    }

    return true;
}

bool BmpBackend::save_file(const std::filesystem::path& path, std::string& err_out) const {
    if (m_raw_file.empty()) {
        err_out = "No BMP data to save";
        return false;
    }

    if (!write_file_atomic(path, m_raw_file.data(), m_raw_file.size())) {
        err_out = "Failed to write BMP file: " + path.string();
        return false;
    }

    return true;
}

size_t BmpBackend::calculate_raw_capacity_bytes() const {
    if (m_dims.width == 0 || m_dims.height == 0) return 0;
    // 3 bits per pixel (B, G, R channels)
    uint64_t total_pixels = static_cast<uint64_t>(m_dims.width) * m_dims.height;
    return static_cast<size_t>((total_pixels * 3) / 8);
}

size_t BmpBackend::calculate_usable_payload_capacity_bytes() const {
    size_t raw = calculate_raw_capacity_bytes();
    return (raw > CONTAINER_HEADER_SIZE) ? (raw - CONTAINER_HEADER_SIZE) : 0;
}

bool BmpBackend::embed_container(const std::vector<uint8_t>& container_bytes, std::string& err_out) {
    size_t max_cap = calculate_raw_capacity_bytes();
    if (container_bytes.size() > max_cap) {
        err_out = "Container size (" + std::to_string(container_bytes.size()) +
                  " bytes) exceeds BMP capacity (" + std::to_string(max_cap) + " bytes)";
        return false;
    }

    size_t byte_idx = 0;
    int bit_idx = 7;
    size_t total_bytes = container_bytes.size();
    size_t bytes_per_pixel = m_bpp / 8;

    uint8_t* raw_data = m_raw_file.data() + m_pixel_offset;

    for (uint32_t y = 0; y < m_dims.height; ++y) {
        uint8_t* row_ptr = raw_data + y * m_row_stride;
        for (uint32_t x = 0; x < m_dims.width; ++x) {
            uint8_t* px = row_ptr + x * bytes_per_pixel;
            // Embed in B, G, R components (indices 0, 1, 2)
            for (int c = 0; c < 3; ++c) {
                if (byte_idx < total_bytes) {
                    uint8_t bit = (container_bytes[byte_idx] >> bit_idx) & 1;
                    px[c] = (px[c] & 0xFE) | bit;

                    bit_idx--;
                    if (bit_idx < 0) {
                        bit_idx = 7;
                        byte_idx++;
                    }
                } else {
                    // Update internal normalized pixel buffer as well
                    for (uint32_t ny = 0; ny < m_dims.height; ++ny) {
                        uint32_t src_y = m_top_down ? ny : (m_dims.height - 1 - ny);
                        const uint8_t* row_src = raw_data + src_y * m_row_stride;
                        for (uint32_t nx = 0; nx < m_dims.width; ++nx) {
                            size_t src_idx = nx * bytes_per_pixel;
                            size_t dst_idx = (ny * m_dims.width + nx) * 3;
                            m_rgb_pixels[dst_idx + 0] = row_src[src_idx + 2]; // R
                            m_rgb_pixels[dst_idx + 1] = row_src[src_idx + 1]; // G
                            m_rgb_pixels[dst_idx + 2] = row_src[src_idx + 0]; // B
                        }
                    }
                    return true;
                }
            }
        }
    }

    return true;
}

bool BmpBackend::extract_container(std::vector<uint8_t>& out_container_bytes, std::string& err_out) const {
    out_container_bytes.clear();
    size_t raw_cap = calculate_raw_capacity_bytes();
    if (raw_cap < CONTAINER_HEADER_SIZE) {
        err_out = "Cover BMP too small for container header";
        return false;
    }

    const uint8_t* raw_data = m_raw_file.data() + m_pixel_offset;
    size_t bytes_per_pixel = m_bpp / 8;

    // Phase 1: Extract 112 header bytes
    std::vector<uint8_t> header_bytes(CONTAINER_HEADER_SIZE, 0);
    size_t byte_idx = 0;
    int bit_idx = 7;

    uint32_t cur_y = 0;
    uint32_t cur_x = 0;
    int cur_c = 0;

    while (byte_idx < CONTAINER_HEADER_SIZE && cur_y < m_dims.height) {
        const uint8_t* px = raw_data + cur_y * m_row_stride + cur_x * bytes_per_pixel;
        uint8_t bit = px[cur_c] & 1;
        header_bytes[byte_idx] |= (bit << bit_idx);

        bit_idx--;
        if (bit_idx < 0) {
            bit_idx = 7;
            byte_idx++;
        }

        cur_c++;
        if (cur_c >= 3) {
            cur_c = 0;
            cur_x++;
            if (cur_x >= m_dims.width) {
                cur_x = 0;
                cur_y++;
            }
        }
    }

    if (byte_idx < CONTAINER_HEADER_SIZE) {
        err_out = "Bitmap ended prematurely before container header";
        return false;
    }

    std::string parse_err;
    auto hdr_opt = parse_container_header(header_bytes.data(), header_bytes.size(), parse_err);
    if (!hdr_opt) {
        err_out = "No RogueByte container found in BMP: " + parse_err;
        return false;
    }

    uint64_t payload_len = hdr_opt->payload_size;
    if (CONTAINER_HEADER_SIZE + payload_len > raw_cap) {
        err_out = "Container reports payload size (" + std::to_string(payload_len) +
                  " bytes) exceeding BMP capacity (" + std::to_string(raw_cap) + " bytes)";
        return false;
    }

    out_container_bytes = std::move(header_bytes);
    out_container_bytes.resize(CONTAINER_HEADER_SIZE + static_cast<size_t>(payload_len));

    byte_idx = CONTAINER_HEADER_SIZE;
    bit_idx = 7;

    while (byte_idx < out_container_bytes.size() && cur_y < m_dims.height) {
        const uint8_t* px = raw_data + cur_y * m_row_stride + cur_x * bytes_per_pixel;
        uint8_t bit = px[cur_c] & 1;
        out_container_bytes[byte_idx] |= (bit << bit_idx);

        bit_idx--;
        if (bit_idx < 0) {
            bit_idx = 7;
            byte_idx++;
        }

        cur_c++;
        if (cur_c >= 3) {
            cur_c = 0;
            cur_x++;
            if (cur_x >= m_dims.width) {
                cur_x = 0;
                cur_y++;
            }
        }
    }

    if (byte_idx < out_container_bytes.size()) {
        err_out = "Unexpected end of BMP pixel data during container extraction";
        return false;
    }

    return true;
}

} // namespace roguebyte
