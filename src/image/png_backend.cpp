#include "roguebyte_stego/png_backend.hpp"
#include "roguebyte_stego/common.hpp"
#include "lodepng/lodepng.h"

#include <cstring>
#include <iostream>

namespace roguebyte {

PngBackend::PngBackend() = default;

bool PngBackend::load_file(const std::filesystem::path& path, std::string& err_out) {
    m_raw_file = read_file_binary(path);
    if (m_raw_file.empty()) {
        err_out = "Cannot open or read PNG file: " + path.string();
        return false;
    }

    unsigned w = 0;
    unsigned h = 0;
    unsigned error = lodepng::decode(m_pixels, w, h, m_raw_file);
    if (error) {
        err_out = std::string("PNG decoding error: ") + lodepng_error_text(error);
        m_pixels.clear();
        m_raw_file.clear();
        return false;
    }

    m_dims.width = w;
    m_dims.height = h;
    m_dims.channels = 4; // RGBA
    m_dims.bit_depth = 8;
    return true;
}

bool PngBackend::save_file(const std::filesystem::path& path, std::string& err_out) const {
    if (m_pixels.empty() || m_dims.width == 0 || m_dims.height == 0) {
        err_out = "No image data to save";
        return false;
    }

    std::vector<uint8_t> encoded;
    unsigned error = lodepng::encode(encoded, m_pixels, m_dims.width, m_dims.height);
    if (error) {
        err_out = std::string("PNG encoding error: ") + lodepng_error_text(error);
        return false;
    }

    if (!write_file_atomic(path, encoded.data(), encoded.size())) {
        err_out = "Failed to write output file atomically: " + path.string();
        return false;
    }

    return true;
}

size_t PngBackend::calculate_raw_capacity_bytes() const {
    if (m_dims.width == 0 || m_dims.height == 0) return 0;
    // 3 color channels (R, G, B) used per pixel, 1 bit per channel = 3 bits per pixel
    uint64_t total_pixels = static_cast<uint64_t>(m_dims.width) * m_dims.height;
    uint64_t total_bits = total_pixels * 3;
    return static_cast<size_t>(total_bits / 8);
}

size_t PngBackend::calculate_usable_payload_capacity_bytes() const {
    size_t raw = calculate_raw_capacity_bytes();
    return (raw > CONTAINER_HEADER_SIZE) ? (raw - CONTAINER_HEADER_SIZE) : 0;
}

bool PngBackend::embed_container(const std::vector<uint8_t>& container_bytes, std::string& err_out) {
    size_t max_cap = calculate_raw_capacity_bytes();
    if (container_bytes.size() > max_cap) {
        err_out = "Payload container size (" + std::to_string(container_bytes.size()) +
                  " bytes) exceeds PNG usable capacity (" + std::to_string(max_cap) + " bytes)";
        return false;
    }

    // Embed bit by bit into R, G, B channels (ignoring A to preserve alpha integrity)
    size_t byte_idx = 0;
    int bit_idx = 7; // MSB to LSB of each byte
    size_t total_bytes = container_bytes.size();

    size_t pixel_count = static_cast<size_t>(m_dims.width) * m_dims.height;
    for (size_t p = 0; p < pixel_count; ++p) {
        size_t base = p * 4;
        for (int c = 0; c < 3; ++c) { // R, G, B
            if (byte_idx < total_bytes) {
                uint8_t cur_byte = container_bytes[byte_idx];
                uint8_t bit = (cur_byte >> bit_idx) & 1;
                m_pixels[base + c] = (m_pixels[base + c] & 0xFE) | bit;

                bit_idx--;
                if (bit_idx < 0) {
                    bit_idx = 7;
                    byte_idx++;
                }
            } else {
                return true;
            }
        }
    }

    return true;
}

bool PngBackend::extract_container(std::vector<uint8_t>& out_container_bytes, std::string& err_out) const {
    out_container_bytes.clear();
    size_t raw_cap = calculate_raw_capacity_bytes();
    if (raw_cap < CONTAINER_HEADER_SIZE) {
        err_out = "Cover PNG too small to contain a RogueByte header";
        return false;
    }

    size_t pixel_count = static_cast<size_t>(m_dims.width) * m_dims.height;

    // Phase 1: Extract 112 header bytes
    std::vector<uint8_t> header_bytes(CONTAINER_HEADER_SIZE, 0);
    size_t byte_idx = 0;
    int bit_idx = 7;
    size_t p_idx = 0;
    int c_idx = 0;

    while (byte_idx < CONTAINER_HEADER_SIZE && p_idx < pixel_count) {
        size_t base = p_idx * 4;
        uint8_t bit = m_pixels[base + c_idx] & 1;
        header_bytes[byte_idx] |= (bit << bit_idx);

        bit_idx--;
        if (bit_idx < 0) {
            bit_idx = 7;
            byte_idx++;
        }

        c_idx++;
        if (c_idx >= 3) {
            c_idx = 0;
            p_idx++;
        }
    }

    if (byte_idx < CONTAINER_HEADER_SIZE) {
        err_out = "Image terminated before complete header could be read";
        return false;
    }

    std::string parse_err;
    auto hdr_opt = parse_container_header(header_bytes.data(), header_bytes.size(), parse_err);
    if (!hdr_opt) {
        err_out = "No RogueByte container found in PNG: " + parse_err;
        return false;
    }

    uint64_t payload_len = hdr_opt->payload_size;
    if (CONTAINER_HEADER_SIZE + payload_len > raw_cap) {
        err_out = "Container header reports payload size (" + std::to_string(payload_len) +
                  " bytes) exceeding total image capacity (" + std::to_string(raw_cap) + " bytes)";
        return false;
    }

    // Phase 2: Extract payload bytes
    out_container_bytes = std::move(header_bytes);
    out_container_bytes.resize(CONTAINER_HEADER_SIZE + static_cast<size_t>(payload_len));

    byte_idx = CONTAINER_HEADER_SIZE;
    bit_idx = 7;

    while (byte_idx < out_container_bytes.size() && p_idx < pixel_count) {
        size_t base = p_idx * 4;
        uint8_t bit = m_pixels[base + c_idx] & 1;
        out_container_bytes[byte_idx] |= (bit << bit_idx);

        bit_idx--;
        if (bit_idx < 0) {
            bit_idx = 7;
            byte_idx++;
        }

        c_idx++;
        if (c_idx >= 3) {
            c_idx = 0;
            p_idx++;
        }
    }

    if (byte_idx < out_container_bytes.size()) {
        err_out = "Unexpected end of pixel data during container extraction";
        return false;
    }

    return true;
}

} // namespace roguebyte
