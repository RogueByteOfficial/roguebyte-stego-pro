#include "roguebyte_stego/jpeg_backend.hpp"
#include "roguebyte_stego/common.hpp"

#include <cstring>
#include <iostream>

namespace roguebyte {

namespace {

constexpr uint16_t MARKER_SOI   = 0xFFD8;
constexpr uint16_t MARKER_EOI   = 0xFFD9;
constexpr uint16_t MARKER_SOF0  = 0xFFC0;
constexpr uint16_t MARKER_SOF2  = 0xFFC2;
constexpr uint16_t MARKER_SOS   = 0xFFDA;
constexpr uint16_t MARKER_APP15 = 0xFFEF;

constexpr uint8_t JPEG_STEGO_MAGIC[9] = {'R', 'B', 'S', '_', 'S', 'T', 'E', 'G', 0x00};
constexpr size_t JPEG_STEGO_MAGIC_LEN = sizeof(JPEG_STEGO_MAGIC);
constexpr size_t MAX_JPEG_SEGMENT_PAYLOAD = 65535 - 2 - JPEG_STEGO_MAGIC_LEN;

} // anonymous namespace

JpegBackend::JpegBackend() = default;

bool JpegBackend::load_file(const std::filesystem::path& path, std::string& err_out) {
    m_raw_file = read_file_binary(path);
    if (m_raw_file.size() < 4) {
        err_out = "File too small to be a valid JPEG image";
        return false;
    }

    if (m_raw_file[0] != 0xFF || m_raw_file[1] != 0xD8) {
        err_out = "Invalid JPEG SOI header (expected 0xFFD8)";
        return false;
    }

    return parse_segments(err_out);
}

bool JpegBackend::parse_segments(std::string& err_out) {
    m_segments.clear();
    const uint8_t* data = m_raw_file.data();
    size_t len = m_raw_file.size();
    size_t offset = 2; // skip SOI

    while (offset < len) {
        if (data[offset] != 0xFF) {
            // Find next marker
            offset++;
            continue;
        }

        while (offset < len && data[offset] == 0xFF) {
            offset++;
        }
        if (offset >= len) break;

        uint8_t marker_code = data[offset++];
        uint16_t marker = 0xFF00 | marker_code;

        if (marker == MARKER_SOI) continue;
        if (marker == MARKER_EOI) {
            JpegSegment seg;
            seg.marker = marker;
            seg.file_offset = offset - 2;
            seg.length = 0;
            m_segments.push_back(seg);
            break;
        }

        // Standalone markers with no payload
        if (marker_code >= 0xD0 && marker_code <= 0xD7) { // RST0 - RST7
            JpegSegment seg;
            seg.marker = marker;
            seg.file_offset = offset - 2;
            seg.length = 0;
            m_segments.push_back(seg);
            continue;
        }

        if (offset + 2 > len) {
            err_out = "Unexpected EOF reading JPEG marker length";
            return false;
        }

        uint16_t seg_len = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
        if (seg_len < 2 || offset + seg_len > len) {
            err_out = "Corrupted JPEG segment length for marker 0x" + to_hex(reinterpret_cast<const uint8_t*>(&marker), 2);
            return false;
        }

        JpegSegment seg;
        seg.marker = marker;
        seg.file_offset = offset - 2;
        seg.length = seg_len;
        seg.data.assign(data + offset + 2, data + offset + seg_len);
        m_segments.push_back(seg);

        // Check if SOF0 / SOF2 to get dimensions
        if (marker == MARKER_SOF0 || marker == MARKER_SOF2) {
            if (seg.data.size() >= 6) {
                m_dims.bit_depth = seg.data[0];
                m_dims.height = (static_cast<uint32_t>(seg.data[1]) << 8) | seg.data[2];
                m_dims.width = (static_cast<uint32_t>(seg.data[3]) << 8) | seg.data[4];
                m_dims.channels = seg.data[5];
            }
        }

        offset += seg_len;

        // If Start of Scan (SOS), entropy coded data follows until EOI
        if (marker == MARKER_SOS) {
            m_sos_offset = offset;
            // Seek forward to find EOI
            while (offset + 1 < len) {
                if (data[offset] == 0xFF && data[offset + 1] == 0xD9) {
                    break;
                }
                offset++;
            }
        }
    }

    if (m_dims.width == 0 || m_dims.height == 0) {
        err_out = "Could not locate valid SOF marker in JPEG";
        return false;
    }

    return true;
}

bool JpegBackend::save_file(const std::filesystem::path& path, std::string& err_out) const {
    if (m_raw_file.empty()) {
        err_out = "No JPEG data to save";
        return false;
    }

    if (!write_file_atomic(path, m_raw_file.data(), m_raw_file.size())) {
        err_out = "Failed to write JPEG file: " + path.string();
        return false;
    }

    return true;
}

size_t JpegBackend::calculate_raw_capacity_bytes() const {
    return MAX_JPEG_SEGMENT_PAYLOAD;
}

size_t JpegBackend::calculate_usable_payload_capacity_bytes() const {
    return (MAX_JPEG_SEGMENT_PAYLOAD > CONTAINER_HEADER_SIZE) ? (MAX_JPEG_SEGMENT_PAYLOAD - CONTAINER_HEADER_SIZE) : 0;
}

bool JpegBackend::embed_container(const std::vector<uint8_t>& container_bytes, std::string& err_out) {
    if (container_bytes.size() > MAX_JPEG_SEGMENT_PAYLOAD) {
        err_out = "Container size (" + std::to_string(container_bytes.size()) +
                  " bytes) exceeds JPEG steganography segment capacity (" +
                  std::to_string(MAX_JPEG_SEGMENT_PAYLOAD) + " bytes)";
        return false;
    }

    // Build new file data by injecting the APP15 segment right after SOI / APP0
    std::vector<uint8_t> new_file;
    new_file.reserve(m_raw_file.size() + container_bytes.size() + 32);

    // Write SOI (2 bytes)
    new_file.push_back(0xFF);
    new_file.push_back(0xD8);

    // Build APP15 segment
    uint16_t seg_length = static_cast<uint16_t>(2 + JPEG_STEGO_MAGIC_LEN + container_bytes.size());
    std::vector<uint8_t> app15_seg;
    app15_seg.push_back(0xFF);
    app15_seg.push_back(0xEF); // APP15
    app15_seg.push_back(static_cast<uint8_t>((seg_length >> 8) & 0xFF));
    app15_seg.push_back(static_cast<uint8_t>(seg_length & 0xFF));
    for (uint8_t b : JPEG_STEGO_MAGIC) {
        app15_seg.push_back(b);
    }
    app15_seg.insert(app15_seg.end(), container_bytes.begin(), container_bytes.end());

    // Insert APP15 after SOI (or after existing APP0)
    bool inserted = false;
    size_t copy_pos = 2;

    for (const auto& seg : m_segments) {
        // Skip any old RogueByte APP15 segments
        if (seg.marker == MARKER_APP15 && seg.data.size() >= JPEG_STEGO_MAGIC_LEN &&
            std::memcmp(seg.data.data(), JPEG_STEGO_MAGIC, JPEG_STEGO_MAGIC_LEN) == 0) {
            // Copy up to this segment start
            if (seg.file_offset > copy_pos) {
                new_file.insert(new_file.end(), m_raw_file.data() + copy_pos, m_raw_file.data() + seg.file_offset);
            }
            // Insert replacement
            new_file.insert(new_file.end(), app15_seg.begin(), app15_seg.end());
            inserted = true;
            copy_pos = seg.file_offset + 2 + seg.length;
            continue;
        }

        // If not inserted yet, insert after APP0 if found or right before DQT/SOF
        if (!inserted && (seg.marker == 0xFFDB || seg.marker == MARKER_SOF0 || seg.marker == MARKER_SOS)) {
            if (seg.file_offset > copy_pos) {
                new_file.insert(new_file.end(), m_raw_file.data() + copy_pos, m_raw_file.data() + seg.file_offset);
            }
            new_file.insert(new_file.end(), app15_seg.begin(), app15_seg.end());
            inserted = true;
            copy_pos = seg.file_offset;
        }
    }

    if (!inserted) {
        // Insert right after SOI
        new_file.insert(new_file.end(), app15_seg.begin(), app15_seg.end());
    }

    // Append remaining file
    if (copy_pos < m_raw_file.size()) {
        new_file.insert(new_file.end(), m_raw_file.data() + copy_pos, m_raw_file.data() + m_raw_file.size());
    }

    m_raw_file = std::move(new_file);
    return parse_segments(err_out);
}

bool JpegBackend::extract_container(std::vector<uint8_t>& out_container_bytes, std::string& err_out) const {
    out_container_bytes.clear();

    for (const auto& seg : m_segments) {
        if (seg.marker == MARKER_APP15 && seg.data.size() >= JPEG_STEGO_MAGIC_LEN + CONTAINER_HEADER_SIZE) {
            if (std::memcmp(seg.data.data(), JPEG_STEGO_MAGIC, JPEG_STEGO_MAGIC_LEN) == 0) {
                const uint8_t* payload_ptr = seg.data.data() + JPEG_STEGO_MAGIC_LEN;
                size_t payload_len = seg.data.size() - JPEG_STEGO_MAGIC_LEN;

                std::string parse_err;
                auto hdr_opt = parse_container_header(payload_ptr, payload_len, parse_err);
                if (!hdr_opt) {
                    err_out = "Invalid RogueByte container in JPEG segment: " + parse_err;
                    return false;
                }

                if (CONTAINER_HEADER_SIZE + hdr_opt->payload_size > payload_len) {
                    err_out = "Truncated container inside JPEG segment";
                    return false;
                }

                out_container_bytes.assign(payload_ptr, payload_ptr + CONTAINER_HEADER_SIZE + hdr_opt->payload_size);
                return true;
            }
        }
    }

    err_out = "No RogueByte steganography container found in JPEG segments";
    return false;
}

} // namespace roguebyte
