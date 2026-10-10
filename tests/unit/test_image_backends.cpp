#include "roguebyte_stego/image_backend.hpp"
#include "roguebyte_stego/png_backend.hpp"
#include "roguebyte_stego/bmp_backend.hpp"
#include "roguebyte_stego/jpeg_backend.hpp"
#include "roguebyte_stego/common.hpp"
#include "lodepng/lodepng.h"

#include <cassert>
#include <iostream>
#include <filesystem>

using namespace roguebyte;

std::filesystem::path create_sample_png(uint32_t w, uint32_t h) {
    std::vector<uint8_t> pixels(w * h * 4);
    for (size_t i = 0; i < w * h; ++i) {
        pixels[i * 4 + 0] = static_cast<uint8_t>((i * 7) % 256); // R
        pixels[i * 4 + 1] = static_cast<uint8_t>((i * 13) % 256); // G
        pixels[i * 4 + 2] = static_cast<uint8_t>((i * 19) % 256); // B
        pixels[i * 4 + 3] = 255;                                  // A
    }

    std::vector<uint8_t> png_bytes;
    lodepng::encode(png_bytes, pixels, w, h);

    std::filesystem::path out_path = "/tmp/rbs_test_cover.png";
    write_file_atomic(out_path, png_bytes.data(), png_bytes.size());
    return out_path;
}

std::filesystem::path create_sample_bmp(uint32_t w, uint32_t h) {
    uint32_t row_stride = ((w * 3 + 3) / 4) * 4;
    uint32_t image_size = row_stride * h;
    uint32_t file_size = 54 + image_size;

    std::vector<uint8_t> bmp(file_size, 0);
    // Header
    bmp[0] = 'B'; bmp[1] = 'M';
    bmp[2] = static_cast<uint8_t>(file_size & 0xFF);
    bmp[3] = static_cast<uint8_t>((file_size >> 8) & 0xFF);
    bmp[4] = static_cast<uint8_t>((file_size >> 16) & 0xFF);
    bmp[5] = static_cast<uint8_t>((file_size >> 24) & 0xFF);
    bmp[10] = 54; // offset
    bmp[14] = 40; // DIB header size
    bmp[18] = static_cast<uint8_t>(w & 0xFF);
    bmp[19] = static_cast<uint8_t>((w >> 8) & 0xFF);
    bmp[22] = static_cast<uint8_t>(h & 0xFF);
    bmp[23] = static_cast<uint8_t>((h >> 8) & 0xFF);
    bmp[26] = 1;  // planes
    bmp[28] = 24; // bpp

    for (uint32_t y = 0; y < h; ++y) {
        uint8_t* row = bmp.data() + 54 + y * row_stride;
        for (uint32_t x = 0; x < w; ++x) {
            row[x * 3 + 0] = static_cast<uint8_t>((x * 3) % 256); // B
            row[x * 3 + 1] = static_cast<uint8_t>((y * 5) % 256); // G
            row[x * 3 + 2] = static_cast<uint8_t>((x + y) % 256); // R
        }
    }

    std::filesystem::path out_path = "/tmp/rbs_test_cover.bmp";
    write_file_atomic(out_path, bmp.data(), bmp.size());
    return out_path;
}

void test_png_backend() {
    auto path = create_sample_png(128, 128);
    PngBackend backend;
    std::string err;
    assert(backend.load_file(path, err));
    assert(backend.dimensions().width == 128);
    assert(backend.dimensions().height == 128);

    size_t raw_cap = backend.calculate_raw_capacity_bytes();
    // 128 * 128 * 3 / 8 = 6144 bytes
    assert(raw_cap == 6144);
    assert(backend.calculate_usable_payload_capacity_bytes() == 6144 - CONTAINER_HEADER_SIZE);

    // Embed test
    std::vector<uint8_t> payload = {'T', 'E', 'S', 'T', '1', '2', '3'};
    auto container = build_container(payload, false, "", false, 0);
    assert(backend.embed_container(container, err));

    std::filesystem::path stego_path = "/tmp/rbs_stego.png";
    assert(backend.save_file(stego_path, err));

    // Reload and extract
    PngBackend reload_backend;
    assert(reload_backend.load_file(stego_path, err));
    std::vector<uint8_t> extracted_container;
    assert(reload_backend.extract_container(extracted_container, err));
    assert(extracted_container == container);

    std::vector<uint8_t> extracted_payload;
    auto parsed = parse_container(extracted_container.data(), extracted_container.size());
    assert(extract_container_payload(parsed, "", extracted_payload, err));
    assert(extracted_payload == payload);

    std::cout << "[PASS] test_png_backend (embed and extract)\n";
}

void test_bmp_backend() {
    auto path = create_sample_bmp(100, 100);
    BmpBackend backend;
    std::string err;
    assert(backend.load_file(path, err));
    assert(backend.dimensions().width == 100);
    assert(backend.dimensions().height == 100);

    size_t raw_cap = backend.calculate_raw_capacity_bytes();
    // 100 * 100 * 3 / 8 = 3750 bytes
    assert(raw_cap == 3750);

    std::vector<uint8_t> payload = {'B', 'M', 'P', '_', 'S', 'E', 'C', 'R', 'E', 'T'};
    auto container = build_container(payload, false, "", false, 0);
    assert(backend.embed_container(container, err));

    std::filesystem::path stego_path = "/tmp/rbs_stego.bmp";
    assert(backend.save_file(stego_path, err));

    // Reload and extract
    BmpBackend reload_backend;
    assert(reload_backend.load_file(stego_path, err));
    std::vector<uint8_t> extracted_container;
    assert(reload_backend.extract_container(extracted_container, err));
    assert(extracted_container == container);

    std::vector<uint8_t> extracted_payload;
    auto parsed = parse_container(extracted_container.data(), extracted_container.size());
    assert(extract_container_payload(parsed, "", extracted_payload, err));
    assert(extracted_payload == payload);

    std::cout << "[PASS] test_bmp_backend (embed and extract)\n";
}

int main() {
    std::cout << "=== Running Unit Tests: Image Backends ===\n";
    test_png_backend();
    test_bmp_backend();
    std::cout << "All image backend unit tests passed!\n";
    return 0;
}
