#pragma once

#include "roguebyte_stego/image_backend.hpp"

namespace roguebyte {

class BmpBackend : public ImageBackend {
public:
    BmpBackend();
    ~BmpBackend() override = default;

    ImageFormat format() const override { return ImageFormat::BMP; }
    std::string format_name() const override { return "BMP (Windows Bitmap)"; }

    bool load_file(const std::filesystem::path& path, std::string& err_out) override;
    bool save_file(const std::filesystem::path& path, std::string& err_out) const override;

    size_t calculate_raw_capacity_bytes() const override;
    size_t calculate_usable_payload_capacity_bytes() const override;

    bool embed_container(const std::vector<uint8_t>& container_bytes, std::string& err_out) override;
    bool extract_container(std::vector<uint8_t>& out_container_bytes, std::string& err_out) const override;

    ImageDimensions dimensions() const override { return m_dims; }
    const std::vector<uint8_t>& raw_pixel_data() const override { return m_rgb_pixels; }
    const std::vector<uint8_t>& raw_file_bytes() const override { return m_raw_file; }

private:
    ImageDimensions m_dims{};
    uint32_t m_pixel_offset{54};
    bool m_top_down{false};
    uint32_t m_row_stride{0};
    uint16_t m_bpp{24};
    std::vector<uint8_t> m_rgb_pixels; // Normalized RGB flat buffer (3 bytes per pixel)
    std::vector<uint8_t> m_raw_file;   // Full file bytes for saving with preserved headers
};

} // namespace roguebyte
