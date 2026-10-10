#include "roguebyte_stego/image_backend.hpp"
#include "roguebyte_stego/common.hpp"
#include "roguebyte_stego/crypto.hpp"
#include "roguebyte_stego/container.hpp"
#include "roguebyte_stego/manifest.hpp"
#include "lodepng/lodepng.h"

#include <cassert>
#include <iostream>
#include <filesystem>

using namespace roguebyte;

void test_png_roundtrip_encrypted() {
    std::cout << "[*] Running Integration Test: PNG Multi-File Encrypted Roundtrip...\n";

    // 1. Create cover PNG (200x200 = 40,000 pixels * 3 / 8 = 15,000 bytes capacity)
    uint32_t w = 200, h = 200;
    std::vector<uint8_t> cover_pixels(w * h * 4);
    for (size_t i = 0; i < w * h; ++i) {
        cover_pixels[i * 4 + 0] = static_cast<uint8_t>((i % 256));
        cover_pixels[i * 4 + 1] = static_cast<uint8_t>((i * 3) % 256);
        cover_pixels[i * 4 + 2] = static_cast<uint8_t>((i * 7) % 256);
        cover_pixels[i * 4 + 3] = 255;
    }
    std::vector<uint8_t> encoded_png;
    lodepng::encode(encoded_png, cover_pixels, w, h);
    std::filesystem::path cover_file = "/tmp/rt_cover.png";
    write_file_atomic(cover_file, encoded_png.data(), encoded_png.size());

    // 2. Create sample payload files
    std::filesystem::path p1 = "/tmp/rt_payload1.txt";
    std::filesystem::path p2 = "/tmp/rt_payload2.bin";
    std::string text_data = "This is a confidential text file for roundtrip testing.\nLine 2.\n";
    write_file_atomic(p1, reinterpret_cast<const uint8_t*>(text_data.data()), text_data.size());

    std::vector<uint8_t> bin_data = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04, 0x55, 0xAA};
    write_file_atomic(p2, bin_data.data(), bin_data.size());

    std::string p1_orig_hash = sha256_file_hex(p1);
    std::string p2_orig_hash = sha256_file_hex(p2);

    // 3. Build manifest
    Manifest manifest;
    std::string err;
    assert(build_manifest_from_files({p1, p2}, manifest, err));
    auto serialized_manifest = serialize_manifest(manifest);

    // 4. Build encrypted container
    std::string password = "TopSecretPassword#2026!";
    auto container = build_container(serialized_manifest, true, password, true, 10000, &err);
    assert(!container.empty());

    // 5. Embed into PNG
    auto backend = create_backend_for_file(cover_file, err);
    assert(backend != nullptr);
    assert(backend->embed_container(container, err));
    std::filesystem::path stego_file = "/tmp/rt_stego.png";
    assert(backend->save_file(stego_file, err));

    // 6. Reload and extract container
    auto reload_backend = create_backend_for_file(stego_file, err);
    assert(reload_backend != nullptr);
    std::vector<uint8_t> extracted_container;
    assert(reload_backend->extract_container(extracted_container, err));
    assert(extracted_container == container);

    auto parsed = parse_container(extracted_container.data(), extracted_container.size());
    assert(parsed.success);

    std::vector<uint8_t> decrypted_manifest_bytes;
    assert(extract_container_payload(parsed, password, decrypted_manifest_bytes, err));

    Manifest extracted_manifest;
    assert(deserialize_manifest(decrypted_manifest_bytes.data(), decrypted_manifest_bytes.size(), extracted_manifest, err));
    assert(extracted_manifest.entries.size() == 2);

    std::filesystem::path out_dir = "/tmp/rt_extracted";
    std::vector<std::string> written_files;
    assert(unpack_manifest(extracted_manifest, out_dir, written_files, err));
    assert(written_files.size() == 2);

    // 7. Verify byte-for-byte SHA256 matches
    std::filesystem::path ext_p1 = out_dir / "rt_payload1.txt";
    std::filesystem::path ext_p2 = out_dir / "rt_payload2.bin";

    std::string ext_p1_hash = sha256_file_hex(ext_p1);
    std::string ext_p2_hash = sha256_file_hex(ext_p2);

    assert(ext_p1_hash == p1_orig_hash);
    assert(ext_p2_hash == p2_orig_hash);

    std::cout << "    [+] Original SHA-256 (File 1): " << p1_orig_hash << "\n";
    std::cout << "    [+] Extracted SHA-256 (File 1): " << ext_p1_hash << "\n";
    std::cout << "    [+] Original SHA-256 (File 2): " << p2_orig_hash << "\n";
    std::cout << "    [+] Extracted SHA-256 (File 2): " << ext_p2_hash << "\n";

    // Clean up
    std::filesystem::remove_all(out_dir);

    std::cout << "[PASS] PNG Multi-File Encrypted Roundtrip passed successfully!\n";
}

int main() {
    std::cout << "=== Running Integration Test: Roundtrip ===\n";
    test_png_roundtrip_encrypted();
    return 0;
}
