#include "roguebyte_stego/version.hpp"
#include "roguebyte_stego/common.hpp"
#include "roguebyte_stego/crypto.hpp"
#include "roguebyte_stego/container.hpp"
#include "roguebyte_stego/manifest.hpp"
#include "roguebyte_stego/image_backend.hpp"
#include "roguebyte_stego/forensics.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

using namespace roguebyte;

namespace {
std::string get_password_input(const std::string& prompt) {
    std::cout << prompt;
    std::cout.flush();

    std::string pass;

#ifdef _WIN32
    int ch;

    while ((ch = _getch()) != '\r' && ch != '\n') {
        if (ch == '\b') {
            if (!pass.empty()) {
                pass.pop_back();
            }
        } else if (ch >= 32 && ch != 127) {
            pass.push_back(static_cast<char>(ch));
        }
    }

    std::cout << '\n';
#else
    termios oldt{}, newt{};
    bool is_term = isatty(STDIN_FILENO) != 0;

    if (is_term && tcgetattr(STDIN_FILENO, &oldt) == 0) {
        newt = oldt;
        newt.c_lflag &= ~ECHO;

        if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) == 0) {
            std::getline(std::cin, pass);
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            std::cout << '\n';
        } else {
            std::getline(std::cin, pass);
        }
    } else {
        std::getline(std::cin, pass);
    }
#endif

    return pass;
}
void print_help() {
    std::cout << "================================================================================\n"
              << " " << ROGUEBYTE_STEGO_PROJECT_NAME << " v" << ROGUEBYTE_STEGO_VERSION_STRING << "\n"
              << " Author: " << ROGUEBYTE_STEGO_AUTHOR << " | Organization: " << ROGUEBYTE_STEGO_ORGANIZATION << "\n"
              << " Cross-Platform Steganography & Digital Forensics CLI\n"
              << "================================================================================\n\n"
              << "USAGE:\n"
              << "  rb-stego <command> [options]\n\n"
              << "COMMANDS:\n"
              << "  embed     Embed arbitrary file(s) into cover image (PNG, BMP, JPEG)\n"
              << "  extract   Extract hidden payload(s) from stego image\n"
              << "  inspect   Perform deep forensic steganalysis on an image\n"
              << "  verify    Compute and verify SHA-256 cryptographic hash of a file\n"
              << "  capacity  Calculate raw and usable steganographic capacity for a cover image\n"
              << "  version   Print application version and build information\n"
              << "  --help    Show this help message\n\n"
              << "EMBED OPTIONS:\n"
              << "  --cover <path>       Cover image (PNG, BMP, JPEG) [required]\n"
              << "  --input <path>...    Payload file(s) to embed (can be specified multiple times)\n"
              << "  --output <path>      Output stego image path [required]\n"
              << "  --encrypt            Enable AES-256-GCM authenticated encryption\n"
              << "  --password <pwd>     Encryption password (prompts securely if omitted)\n"
              << "  --iterations <num>   PBKDF2 key derivation iterations (default: 100000)\n"
              << "  --force              Overwrite existing output file without confirmation\n\n"
              << "EXTRACT OPTIONS:\n"
              << "  --input <path>       Stego image path [required]\n"
              << "  --output-dir <dir>   Target directory for extracted payload(s) [required]\n"
              << "  --password <pwd>     Decryption password (prompts securely if required)\n"
              << "  --force              Overwrite existing output files\n\n"
              << "INSPECT OPTIONS:\n"
              << "  --input <path>       Image file to analyze [required]\n"
              << "  --json               Output report in machine-readable JSON format\n\n"
              << "VERIFY OPTIONS:\n"
              << "  --input <path>       File to hash [required]\n"
              << "  --expected <sha256>  Expected SHA-256 hash in hexadecimal\n\n"
              << "CAPACITY OPTIONS:\n"
              << "  --cover <path>       Cover image to evaluate [required]\n\n"
              << "EXIT CODES:\n"
              << "  0 = Success\n"
              << "  1 = General Error\n"
              << "  2 = Invalid Arguments\n"
              << "  3 = Cover Error (not found, corrupted, or unsupported)\n"
              << "  4 = Payload Error (not found, unreadable)\n"
              << "  5 = Insufficient Capacity\n"
              << "  6 = Authentication / Decryption Failed\n"
              << "  7 = Integrity Verification Failed\n"
              << "  8 = Security Violation (path traversal)\n";
}

int handle_version() {
    std::cout << ROGUEBYTE_STEGO_PROJECT_NAME << " version " << ROGUEBYTE_STEGO_VERSION_STRING << "\n"
              << "Author       : " << ROGUEBYTE_STEGO_AUTHOR << "\n"
              << "Organization : " << ROGUEBYTE_STEGO_ORGANIZATION << "\n"
              << "Standard     : C++20 / CMake / OpenSSL 3.0 EVP\n"
              << "Backends     : PNG (Lossless Pixel LSB), BMP (Lossless Pixel LSB), JPEG (Structural Segment Carrier)\n";
    return static_cast<int>(ExitCode::Success);
}

int handle_capacity(int argc, char* argv[]) {
    std::filesystem::path cover_path;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--cover" && i + 1 < argc) {
            cover_path = argv[++i];
        }
    }

    if (cover_path.empty()) {
        std::cerr << "[-] Error: Missing required option --cover <path>\n";
        return static_cast<int>(ExitCode::InvalidArguments);
    }

    std::string err;
    auto backend = create_backend_for_file(cover_path, err);
    if (!backend) {
        std::cerr << "[-] Error: " << err << "\n";
        return static_cast<int>(ExitCode::CoverError);
    }

    auto dims = backend->dimensions();
    size_t raw_cap = backend->calculate_raw_capacity_bytes();
    size_t usable_cap = backend->calculate_usable_payload_capacity_bytes();

    std::cout << "Cover File     : " << cover_path.string() << "\n"
              << "Format         : " << backend->format_name() << "\n"
              << "Dimensions     : " << dims.width << " x " << dims.height
              << " (" << dims.channels << " channels, " << dims.bit_depth << "-bit)\n"
              << "Raw Capacity   : " << format_bytes(raw_cap) << " (" << raw_cap << " bytes)\n"
              << "Usable Payload : " << format_bytes(usable_cap) << " (" << usable_cap << " bytes)\n"
              << "Container Ovrhd: " << CONTAINER_HEADER_SIZE << " bytes (RBS1 header)\n";

    return static_cast<int>(ExitCode::Success);
}

int handle_verify(int argc, char* argv[]) {
    std::filesystem::path input_path;
    std::string expected_hash;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) {
            input_path = argv[++i];
        } else if (arg == "--expected" && i + 1 < argc) {
            expected_hash = argv[++i];
        }
    }

    if (input_path.empty()) {
        std::cerr << "[-] Error: Missing required option --input <path>\n";
        return static_cast<int>(ExitCode::InvalidArguments);
    }

    if (!std::filesystem::exists(input_path)) {
        std::cerr << "[-] Error: File not found: " << input_path.string() << "\n";
        return static_cast<int>(ExitCode::PayloadError);
    }

    std::string hash = sha256_file_hex(input_path);
    if (hash.empty()) {
        std::cerr << "[-] Error: Failed to compute SHA-256 for: " << input_path.string() << "\n";
        return static_cast<int>(ExitCode::GeneralError);
    }

    std::cout << "Target File : " << input_path.string() << "\n"
              << "SHA-256     : " << hash << "\n";

    if (!expected_hash.empty()) {
        std::string exp = expected_hash;
        std::transform(exp.begin(), exp.end(), exp.begin(), ::tolower);
        std::transform(hash.begin(), hash.end(), hash.begin(), ::tolower);

        if (exp == hash) {
            std::cout << "[+] INTEGRITY VERIFIED: SHA-256 matches expected checksum.\n";
            return static_cast<int>(ExitCode::Success);
        } else {
            std::cerr << "[-] INTEGRITY MISMATCH: Expected " << exp << ", computed " << hash << "\n";
            return static_cast<int>(ExitCode::IntegrityFailed);
        }
    }

    return static_cast<int>(ExitCode::Success);
}

int handle_inspect(int argc, char* argv[]) {
    std::filesystem::path input_path;
    bool json_mode = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) {
            input_path = argv[++i];
        } else if (arg == "--json") {
            json_mode = true;
        }
    }

    if (input_path.empty()) {
        std::cerr << "[-] Error: Missing required option --input <path>\n";
        return static_cast<int>(ExitCode::InvalidArguments);
    }

    if (!std::filesystem::exists(input_path)) {
        std::cerr << "[-] Error: File not found: " << input_path.string() << "\n";
        return static_cast<int>(ExitCode::CoverError);
    }

    auto report = inspect_image(input_path);
    if (json_mode) {
        std::cout << report.to_json() << "\n";
    } else {
        std::cout << report.to_text_summary() << "\n";
    }

    if (!report.structurally_valid) {
        return static_cast<int>(ExitCode::CoverError);
    }

    return static_cast<int>(ExitCode::Success);
}

int handle_embed(int argc, char* argv[]) {
    std::filesystem::path cover_path;
    std::vector<std::filesystem::path> input_paths;
    std::filesystem::path output_path;
    bool encrypt = false;
    std::string password;
    uint32_t iterations = DEFAULT_KDF_ITERATIONS;
    bool force = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--cover" && i + 1 < argc) {
            cover_path = argv[++i];
        } else if (arg == "--input" && i + 1 < argc) {
            input_paths.push_back(argv[++i]);
        } else if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "--encrypt") {
            encrypt = true;
        } else if (arg == "--password" && i + 1 < argc) {
            password = argv[++i];
            encrypt = true;
        } else if (arg == "--iterations" && i + 1 < argc) {
            iterations = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--force") {
            force = true;
        }
    }

    if (cover_path.empty() || input_paths.empty() || output_path.empty()) {
        std::cerr << "[-] Error: Missing required arguments. Expected --cover, --input, and --output.\n";
        return static_cast<int>(ExitCode::InvalidArguments);
    }

    if (std::filesystem::exists(output_path) && !force) {
        std::cerr << "[!] Warning: Output file exists: " << output_path.string() << ". Use --force to overwrite.\n";
        return static_cast<int>(ExitCode::GeneralError);
    }

    if (encrypt && password.empty()) {
        password = get_password_input("Enter encryption password: ");
        if (password.empty()) {
            std::cerr << "[-] Error: Password cannot be empty.\n";
            return static_cast<int>(ExitCode::AuthenticationFailed);
        }
        std::string confirm = get_password_input("Confirm encryption password: ");
        if (password != confirm) {
            std::cerr << "[-] Error: Passwords do not match.\n";
            return static_cast<int>(ExitCode::AuthenticationFailed);
        }
    }

    // 1. Load cover image
    std::cout << "[*] Loading cover image: " << cover_path.string() << " ...\n";
    std::string err;
    auto backend = create_backend_for_file(cover_path, err);
    if (!backend) {
        std::cerr << "[-] Error: " << err << "\n";
        return static_cast<int>(ExitCode::CoverError);
    }

    size_t usable_cap = backend->calculate_usable_payload_capacity_bytes();
    std::cout << "[*] Usable cover capacity: " << format_bytes(usable_cap) << " (" << usable_cap << " bytes)\n";

    // 2. Build payload manifest
    std::cout << "[*] Building archive manifest for " << input_paths.size() << " payload file(s)...\n";
    Manifest manifest;
    if (!build_manifest_from_files(input_paths, manifest, err)) {
        std::cerr << "[-] Error reading payload files: " << err << "\n";
        return static_cast<int>(ExitCode::PayloadError);
    }

    for (const auto& entry : manifest.entries) {
        std::cout << "    -> " << entry.safe_filename << " (" << format_bytes(entry.file_size)
                  << ", SHA-256: " << to_hex(entry.sha256.data(), 8) << "...)\n";
    }

    std::vector<uint8_t> serialized_manifest = serialize_manifest(manifest);

    // 3. Check capacity before encryption or modification
    // RBS1 header is 112 bytes
    size_t total_container_estimate = CONTAINER_HEADER_SIZE + serialized_manifest.size();
    if (total_container_estimate > backend->calculate_raw_capacity_bytes()) {
        std::cerr << "[-] Error: Insufficient capacity! Payload (" << format_bytes(total_container_estimate)
                  << ") exceeds cover capacity (" << format_bytes(backend->calculate_raw_capacity_bytes()) << ")\n";
        return static_cast<int>(ExitCode::InsufficientCapacity);
    }

    // 4. Build authenticated RogueByte container
    std::cout << "[*] Constructing container (" << (encrypt ? "AES-256-GCM Encrypted" : "Plaintext") << ")...\n";
    std::string build_err;
    std::vector<uint8_t> container_bytes = build_container(
        serialized_manifest, encrypt, password, true, iterations, &build_err
    );

    if (container_bytes.empty()) {
        std::cerr << "[-] Container build error: " << build_err << "\n";
        return static_cast<int>(ExitCode::GeneralError);
    }

    // 5. Embed into image
    std::cout << "[*] Embedding container into " << backend->format_name() << " ...\n";
    if (!backend->embed_container(container_bytes, err)) {
        std::cerr << "[-] Embedding failed: " << err << "\n";
        return static_cast<int>(ExitCode::GeneralError);
    }

    // 6. Atomically save output
    std::cout << "[*] Writing output stego file to: " << output_path.string() << " ...\n";
    if (!backend->save_file(output_path, err)) {
        std::cerr << "[-] Save failed: " << err << "\n";
        return static_cast<int>(ExitCode::GeneralError);
    }

    std::string out_hash = sha256_file_hex(output_path);
    std::cout << "[+] SUCCESS: Payload embedded successfully.\n"
              << "    Output File : " << output_path.string() << "\n"
              << "    Output SHA256: " << out_hash << "\n"
              << "    Capacity Used: " << format_bytes(container_bytes.size()) << " / "
              << format_bytes(backend->calculate_raw_capacity_bytes()) << "\n";

    return static_cast<int>(ExitCode::Success);
}

int handle_extract(int argc, char* argv[]) {
    std::filesystem::path input_path;
    std::filesystem::path output_dir;
    std::string password;
    bool force = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) {
            input_path = argv[++i];
        } else if (arg == "--output-dir" && i + 1 < argc) {
            output_dir = argv[++i];
        } else if (arg == "--password" && i + 1 < argc) {
            password = argv[++i];
        } else if (arg == "--force") {
            force = true;
        }
    }

    if (input_path.empty() || output_dir.empty()) {
        std::cerr << "[-] Error: Missing required arguments. Expected --input and --output-dir.\n";
        return static_cast<int>(ExitCode::InvalidArguments);
    }

    if (!std::filesystem::exists(input_path)) {
        std::cerr << "[-] Error: Stego file not found: " << input_path.string() << "\n";
        return static_cast<int>(ExitCode::CoverError);
    }

    std::cout << "[*] Loading stego image: " << input_path.string() << " ...\n";
    std::string err;
    auto backend = create_backend_for_file(input_path, err);
    if (!backend) {
        std::cerr << "[-] Error: " << err << "\n";
        return static_cast<int>(ExitCode::CoverError);
    }

    std::vector<uint8_t> raw_container;
    if (!backend->extract_container(raw_container, err)) {
        std::cerr << "[-] Extraction failed: " << err << "\n";
        return static_cast<int>(ExitCode::GeneralError);
    }

    std::string parse_err;
    auto parsed = parse_container(raw_container.data(), raw_container.size());
    if (!parsed.success) {
        std::cerr << "[-] Malformed container: " << parsed.error_message << "\n";
        return static_cast<int>(ExitCode::GeneralError);
    }

    std::cout << "[*] Found RogueByte Container v" << static_cast<int>(parsed.header.version) << "\n"
              << "    Status   : " << (parsed.header.is_encrypted() ? "Encrypted (AES-256-GCM)" : "Plaintext") << "\n"
              << "    Payload  : " << format_bytes(parsed.header.payload_size) << "\n";

    if (parsed.header.is_encrypted() && password.empty()) {
        password = get_password_input("Enter decryption password: ");
        if (password.empty()) {
            std::cerr << "[-] Error: Password is required for encrypted container.\n";
            return static_cast<int>(ExitCode::AuthenticationFailed);
        }
    }

    std::vector<uint8_t> decrypted_payload;
    std::string extract_err;
    if (!extract_container_payload(parsed, password, decrypted_payload, extract_err)) {
        std::cerr << "[-] Decryption/Integrity failure: " << extract_err << "\n";
        return static_cast<int>(ExitCode::AuthenticationFailed);
    }

    // Unpack payload
    if (parsed.header.is_multi_file() || has_manifest_magic(decrypted_payload.data(), decrypted_payload.size())) {
        Manifest manifest;
        std::string man_err;
        if (!deserialize_manifest(decrypted_payload.data(), decrypted_payload.size(), manifest, man_err)) {
            std::cerr << "[-] Manifest deserialization failed: " << man_err << "\n";
            return static_cast<int>(ExitCode::GeneralError);
        }

        std::vector<std::string> written;
        std::string unpack_err;
        if (!unpack_manifest(manifest, output_dir, written, unpack_err)) {
            std::cerr << "[-] Extraction error: " << unpack_err << "\n";
            return static_cast<int>(ExitCode::SecurityViolation);
        }

        std::cout << "[+] SUCCESS: Extracted " << written.size() << " payload file(s) to " << output_dir.string() << ":\n";
        for (const auto& w : written) {
            std::cout << "    [+] " << w << " (SHA-256: " << sha256_file_hex(w) << ")\n";
        }
    } else {
        // Single file raw fallback
        std::filesystem::path single_target = output_dir / "extracted_payload.bin";
        if (!write_file_atomic(single_target, decrypted_payload.data(), decrypted_payload.size())) {
            std::cerr << "[-] Failed to write single extracted payload.\n";
            return static_cast<int>(ExitCode::GeneralError);
        }
        std::cout << "[+] SUCCESS: Extracted single payload to " << single_target.string() << "\n";
    }

    return static_cast<int>(ExitCode::Success);
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return static_cast<int>(ExitCode::InvalidArguments);
    }

    std::string cmd = argv[1];
    if (cmd == "version" || cmd == "--version" || cmd == "-v") {
        return handle_version();
    } else if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        print_help();
        return static_cast<int>(ExitCode::Success);
    } else if (cmd == "capacity") {
        return handle_capacity(argc, argv);
    } else if (cmd == "verify") {
        return handle_verify(argc, argv);
    } else if (cmd == "inspect") {
        return handle_inspect(argc, argv);
    } else if (cmd == "embed") {
        return handle_embed(argc, argv);
    } else if (cmd == "extract") {
        return handle_extract(argc, argv);
    } else {
        std::cerr << "[-] Unknown command: " << cmd << "\n";
        print_help();
        return static_cast<int>(ExitCode::InvalidArguments);
    }
}
