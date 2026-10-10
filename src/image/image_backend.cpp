#include "roguebyte_stego/image_backend.hpp"
#include "roguebyte_stego/png_backend.hpp"
#include "roguebyte_stego/bmp_backend.hpp"
#include "roguebyte_stego/jpeg_backend.hpp"
#include "roguebyte_stego/common.hpp"

#include <fstream>
#include <algorithm>

namespace roguebyte {

ImageFormat detect_image_format_from_bytes(const uint8_t* data, size_t len) {
    if (!data || len < 4) return ImageFormat::Unknown;

    // PNG: 89 50 4E 47 0D 0A 1A 0A
    if (len >= 8 && data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47 &&
        data[4] == 0x0D && data[5] == 0x0A && data[6] == 0x1A && data[7] == 0x0A) {
        return ImageFormat::PNG;
    }

    // BMP: 'B' 'M'
    if (data[0] == 'B' && data[1] == 'M') {
        return ImageFormat::BMP;
    }

    // JPEG: FF D8 FF
    if (len >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
        return ImageFormat::JPEG;
    }

    return ImageFormat::Unknown;
}

ImageFormat detect_image_format(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return ImageFormat::Unknown;

    uint8_t magic[8] = {0};
    file.read(reinterpret_cast<char*>(magic), sizeof(magic));
    size_t count = static_cast<size_t>(file.gcount());

    auto fmt = detect_image_format_from_bytes(magic, count);
    if (fmt != ImageFormat::Unknown) {
        return fmt;
    }

    // Fallback to extension if magic was borderline
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".png") return ImageFormat::PNG;
    if (ext == ".bmp") return ImageFormat::BMP;
    if (ext == ".jpg" || ext == ".jpeg") return ImageFormat::JPEG;

    return ImageFormat::Unknown;
}

std::unique_ptr<ImageBackend> create_backend(ImageFormat fmt) {
    switch (fmt) {
        case ImageFormat::PNG:
            return std::make_unique<PngBackend>();
        case ImageFormat::BMP:
            return std::make_unique<BmpBackend>();
        case ImageFormat::JPEG:
            return std::make_unique<JpegBackend>();
        default:
            return nullptr;
    }
}

std::unique_ptr<ImageBackend> create_backend_for_file(const std::filesystem::path& path, std::string& err_out) {
    ImageFormat fmt = detect_image_format(path);
    if (fmt == ImageFormat::Unknown) {
        err_out = "Unsupported or unrecognized image format for file: " + path.string() +
                  " (Supported formats: PNG, BMP, JPEG)";
        return nullptr;
    }

    auto backend = create_backend(fmt);
    if (!backend) {
        err_out = "Failed to instantiate backend for format";
        return nullptr;
    }

    if (!backend->load_file(path, err_out)) {
        return nullptr;
    }

    return backend;
}

} // namespace roguebyte
