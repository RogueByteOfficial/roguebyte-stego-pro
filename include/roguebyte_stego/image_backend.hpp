#pragma once

#include "roguebyte_stego/container.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

namespace roguebyte {

enum class ImageFormat {
    Unknown,
    PNG,
    BMP,
    JPEG
};

struct ImageDimensions {
    uint32_t width{0};
    uint32_t height{0};
    uint32_t channels{0};
    uint32_t bit_depth{8};
};

class ImageBackend {
public:
    virtual ~ImageBackend() = default;

    virtual ImageFormat format() const = 0;
    virtual std::string format_name() const = 0;

    virtual bool load_file(const std::filesystem::path& path, std::string& err_out) = 0;
    virtual bool save_file(const std::filesystem::path& path, std::string& err_out) const = 0;

    // Capacity calculations
    virtual size_t calculate_raw_capacity_bytes() const = 0;
    virtual size_t calculate_usable_payload_capacity_bytes() const = 0;

    // Steganography operations
    virtual bool embed_container(const std::vector<uint8_t>& container_bytes, std::string& err_out) = 0;
    virtual bool extract_container(std::vector<uint8_t>& out_container_bytes, std::string& err_out) const = 0;

    // Metadata & Pixel inspection
    virtual ImageDimensions dimensions() const = 0;
    virtual const std::vector<uint8_t>& raw_pixel_data() const = 0;
    virtual const std::vector<uint8_t>& raw_file_bytes() const = 0;
};

// Factory functions
ImageFormat detect_image_format(const std::filesystem::path& path);
ImageFormat detect_image_format_from_bytes(const uint8_t* data, size_t len);

std::unique_ptr<ImageBackend> create_backend(ImageFormat fmt);
std::unique_ptr<ImageBackend> create_backend_for_file(const std::filesystem::path& path, std::string& err_out);

} // namespace roguebyte
