#include "roguebyte_stego/manifest.hpp"
#include "roguebyte_stego/common.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>

using namespace roguebyte;

void test_manifest_roundtrip() {
    Manifest manifest;
    ManifestEntry e1;
    e1.original_filename = "document.txt";
    e1.safe_filename = "document.txt";
    std::string s1 = "This is a secret document content.";
    e1.data.assign(s1.begin(), s1.end());
    e1.file_size = e1.data.size();
    e1.mtime_sec = 1700000000;
    e1.sha256 = sha256_digest(e1.data.data(), e1.data.size());

    ManifestEntry e2;
    e2.original_filename = "binary_tool.exe";
    e2.safe_filename = "binary_tool.exe";
    uint8_t bin[] = {0x4D, 0x5A, 0x90, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00};
    e2.data.assign(bin, bin + sizeof(bin));
    e2.file_size = e2.data.size();
    e2.mtime_sec = 1700000001;
    e2.sha256 = sha256_digest(e2.data.data(), e2.data.size());

    manifest.entries.push_back(e1);
    manifest.entries.push_back(e2);

    auto serialized = serialize_manifest(manifest);
    assert(has_manifest_magic(serialized.data(), serialized.size()));

    Manifest parsed;
    std::string err;
    assert(deserialize_manifest(serialized.data(), serialized.size(), parsed, err));
    assert(parsed.entries.size() == 2);
    assert(parsed.entries[0].safe_filename == "document.txt");
    assert(parsed.entries[0].data == e1.data);
    assert(parsed.entries[1].safe_filename == "binary_tool.exe");
    assert(parsed.entries[1].data == e2.data);

    std::cout << "[PASS] test_manifest_roundtrip\n";
}

void test_path_traversal_defense() {
    // Malicious manifest entry with directory traversal attempt
    Manifest manifest;
    ManifestEntry evil;
    evil.original_filename = "../../etc/shadow";
    evil.safe_filename = sanitize_basename(evil.original_filename);
    std::string exploit = "root:*:0:0:root:/root:/bin/bash";
    evil.data.assign(exploit.begin(), exploit.end());
    evil.file_size = evil.data.size();
    evil.mtime_sec = 12345;
    evil.sha256 = sha256_digest(evil.data.data(), evil.data.size());

    assert(evil.safe_filename == "shadow"); // Stripped dangerous directory traversal!
    assert(evil.safe_filename.find('/') == std::string::npos);
    assert(evil.safe_filename.find("..") == std::string::npos);

    manifest.entries.push_back(evil);

    std::filesystem::path test_dir = "/tmp/rbs_test_unpack";
    std::vector<std::string> written;
    std::string err;
    assert(unpack_manifest(manifest, test_dir, written, err));
    assert(written.size() == 1);
    assert(written[0] == (test_dir / "shadow").string());

    // Clean up
    std::filesystem::remove_all(test_dir);

    std::cout << "[PASS] test_path_traversal_defense\n";
}

int main() {
    std::cout << "=== Running Unit Tests: Manifest ===\n";
    test_manifest_roundtrip();
    test_path_traversal_defense();
    std::cout << "All manifest unit tests passed!\n";
    return 0;
}
