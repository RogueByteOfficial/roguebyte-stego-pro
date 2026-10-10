#pragma once

#include "roguebyte_stego/image_backend.hpp"

namespace roguebyte {

class PngBackend : public ImageBackend {
public:
    PngBackend();
    ~PngBackend() override = default;

    ImageFormat format() const override { return ImageFormat::PNG; }
    std::string format_name() const override { return "PNG (Portable Network Graphics)"; }

    bool load_file(const std::filesystem::path& path, std::string& err_out) override;
    bool save_file(const std::filesystem::path& path, std::string& err_out) const override;

    size_t calculate_raw_capacity_bytes() const override;
    size_t calculate_usable_payload_capacity_bytes() const override;

    bool embed_container(const std::vector<uint8_t>& container_bytes, std::string& err_out) override;
    bool extract_container(std::vector<uint8_t>& out_container_bytes, std::string& err_out) const override;

    ImageDimensions dimensions() const override { return m_dims; }
    const std::vector<uint8_t>& raw_pixel_data() const override { return m_pixels; }
    const std::vector<uint8_t>& raw_file_bytes() const override { return m_raw_file; }

private:
    ImageDimensions m_dims{};
    std::vector<uint8_t> m_pixels; // 4 bytes per pixel: R, G, B, A
    std::vector<uint8_t> m_raw_file;
};

} // namespace roguebyte
