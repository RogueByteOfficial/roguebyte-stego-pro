#include "roguebyte_stego/forensics.hpp"
#include "roguebyte_stego/image_backend.hpp"
#include "roguebyte_stego/container.hpp"
#include "roguebyte_stego/common.hpp"
#include "lodepng/lodepng.h"

#include <cassert>
#include <iostream>
#include <filesystem>

using namespace roguebyte;

void test_forensics_inspection() {
    std::cout << "[*] Running Forensics Inspection Integration Test...\n";

    // 1. Create clean image
    uint32_t w = 64, h = 64;
    std::vector<uint8_t> pixels(w * h * 4);
    for (size_t i = 0; i < w * h; ++i) {
        pixels[i * 4 + 0] = static_cast<uint8_t>(i % 128);
        pixels[i * 4 + 1] = static_cast<uint8_t>((i * 2) % 128);
        pixels[i * 4 + 2] = static_cast<uint8_t>((i * 3) % 128);
        pixels[i * 4 + 3] = 255;
    }
    std::vector<uint8_t> clean_png;
    lodepng::encode(clean_png, pixels, w, h);
    std::filesystem::path clean_path = "/tmp/forensics_clean.png";
    write_file_atomic(clean_path, clean_png.data(), clean_png.size());

    // Inspect clean image
    auto clean_report = inspect_image(clean_path);
    assert(clean_report.structurally_valid);
    assert(!clean_report.container_detected);
    assert(clean_report.verdict == ForensicVerdict::CleanOrInconclusive);

    // 2. Embed RogueByte container into image
    std::string err;
    auto backend = create_backend_for_file(clean_path, err);
    assert(backend != nullptr);

    std::vector<uint8_t> secret = {'R', 'O', 'G', 'U', 'E', '_', 'S', 'E', 'C', 'R', 'E', 'T'};
    auto container = build_container(secret, true, "ForensicPass#1", false, 5000);
    assert(backend->embed_container(container, err));

    std::filesystem::path stego_path = "/tmp/forensics_stego.png";
    assert(backend->save_file(stego_path, err));

    // Inspect stego image
    auto stego_report = inspect_image(stego_path);
    assert(stego_report.structurally_valid);
    assert(stego_report.container_detected);
    assert(stego_report.verdict == ForensicVerdict::ConfirmedRogueByteContainer);
    assert(stego_report.confidence == ForensicConfidence::ConfirmedContainer);
    assert(stego_report.container_header.is_encrypted());
    assert(stego_report.container_header.payload_size == secret.size());

    std::cout << "[PASS] Forensics inspection correctly distinguishes clean vs stego images.\n";
}

int main() {
    std::cout << "=== Running Integration Test: Forensics ===\n";
    test_forensics_inspection();
    return 0;
}
