#pragma once

#include "roguebyte_stego/image_backend.hpp"

namespace roguebyte {

struct JpegSegment {
    uint16_t marker{0};
    size_t file_offset{0};
    uint16_t length{0}; // Length as stored in marker (excluding 2-byte marker code)
    std::vector<uint8_t> data;
};

class JpegBackend : public ImageBackend {
public:
    JpegBackend();
    ~JpegBackend() override = default;

    ImageFormat format() const override { return ImageFormat::JPEG; }
    std::string format_name() const override { return "JPEG (Joint Photographic Experts Group)"; }

    bool load_file(const std::filesystem::path& path, std::string& err_out) override;
    bool save_file(const std::filesystem::path& path, std::string& err_out) const override;

    size_t calculate_raw_capacity_bytes() const override;
    size_t calculate_usable_payload_capacity_bytes() const override;

    bool embed_container(const std::vector<uint8_t>& container_bytes, std::string& err_out) override;
    bool extract_container(std::vector<uint8_t>& out_container_bytes, std::string& err_out) const override;

    ImageDimensions dimensions() const override { return m_dims; }
    const std::vector<uint8_t>& raw_pixel_data() const override { return m_empty_pixels; }
    const std::vector<uint8_t>& raw_file_bytes() const override { return m_raw_file; }

    const std::vector<JpegSegment>& segments() const { return m_segments; }

private:
    bool parse_segments(std::string& err_out);

    ImageDimensions m_dims{};
    std::vector<uint8_t> m_raw_file;
    std::vector<uint8_t> m_empty_pixels;
    std::vector<JpegSegment> m_segments;
    size_t m_sos_offset{0};
};

} // namespace roguebyte
