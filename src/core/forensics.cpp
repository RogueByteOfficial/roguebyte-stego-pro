#include "roguebyte_stego/forensics.hpp"
#include "roguebyte_stego/common.hpp"
#include "roguebyte_stego/crypto.hpp"

#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace roguebyte {

namespace {

double compute_binary_entropy(size_t zeros, size_t ones) {
    size_t total = zeros + ones;
    if (total == 0 || zeros == 0 || ones == 0) return 0.0;
    double p0 = static_cast<double>(zeros) / total;
    double p1 = static_cast<double>(ones) / total;
    return -(p0 * std::log2(p0) + p1 * std::log2(p1));
}

BitplaneAnalysis analyze_bitplanes(const std::vector<uint8_t>& pixels, uint32_t width, uint32_t height) {
    BitplaneAnalysis res{};
    size_t pixel_count = static_cast<size_t>(width) * height;
    if (pixel_count == 0 || pixels.size() < pixel_count * 3) {
        return res;
    }

    // Pixels are assumed to have at least RGB layout (e.g. 3 bytes or RGBA 4 bytes)
    size_t stride = (pixels.size() >= pixel_count * 4) ? 4 : 3;

    for (int b = 0; b < 8; ++b) {
        size_t r_zeros = 0, r_ones = 0;
        size_t g_zeros = 0, g_ones = 0;
        size_t b_zeros = 0, b_ones = 0;

        for (size_t i = 0; i < pixel_count; ++i) {
            size_t base = i * stride;
            uint8_t r = pixels[base + 0];
            uint8_t g = pixels[base + 1];
            uint8_t blue = pixels[base + 2];

            ((r >> b) & 1) ? r_ones++ : r_zeros++;
            ((g >> b) & 1) ? g_ones++ : g_zeros++;
            ((blue >> b) & 1) ? b_ones++ : b_zeros++;
        }

        res.red_entropy[b] = compute_binary_entropy(r_zeros, r_ones);
        res.green_entropy[b] = compute_binary_entropy(g_zeros, g_ones);
        res.blue_entropy[b] = compute_binary_entropy(b_zeros, b_ones);
    }

    res.average_lsb_entropy = (res.red_entropy[0] + res.green_entropy[0] + res.blue_entropy[0]) / 3.0;
    return res;
}

ChiSquareAnalysis perform_chi_square(const std::vector<uint8_t>& pixels, uint32_t width, uint32_t height) {
    ChiSquareAnalysis res{};
    size_t pixel_count = static_cast<size_t>(width) * height;
    if (pixel_count < 100 || pixels.size() < pixel_count * 3) {
        return res;
    }

    size_t stride = (pixels.size() >= pixel_count * 4) ? 4 : 3;
    uint64_t hist[256] = {0};

    for (size_t i = 0; i < pixel_count; ++i) {
        size_t base = i * stride;
        hist[pixels[base + 0]]++;
        hist[pixels[base + 1]]++;
        hist[pixels[base + 2]]++;
    }

    double chi2 = 0.0;
    int k_pairs = 0;

    for (int k = 0; k < 128; ++k) {
        uint64_t c0 = hist[2 * k];
        uint64_t c1 = hist[2 * k + 1];
        uint64_t sum = c0 + c1;
        if (sum > 5) { // Minimum sample for chi-square validity
            double expected = sum / 2.0;
            double diff0 = c0 - expected;
            double diff1 = c1 - expected;
            chi2 += (diff0 * diff0) / expected + (diff1 * diff1) / expected;
            k_pairs++;
        }
    }

    res.chi_square_stat = chi2;
    res.degrees_of_freedom = (k_pairs > 1) ? (k_pairs - 1) : 1;

    // Approximation of p-value or normalized anomaly
    double normalized = chi2 / (res.degrees_of_freedom > 0 ? res.degrees_of_freedom : 1.0);
    // In steganography with random LSBs, chi-square statistic is small (c0 approx c1), so normalized < 0.5 suggests equalization
    res.statistically_anomalous = (normalized < 0.35 && k_pairs > 30);
    res.p_value = normalized;

    return res;
}

} // anonymous namespace

ForensicReport inspect_image(const std::filesystem::path& path) {
    ForensicReport report;
    report.file_path = path;
    report.file_size = get_file_size(path);
    report.file_sha256 = sha256_file_hex(path);

    std::string err;
    auto backend = create_backend_for_file(path, err);
    if (!backend) {
        report.structurally_valid = false;
        report.structural_notes = "Failed to load image: " + err;
        report.verdict = ForensicVerdict::CleanOrInconclusive;
        report.confidence = ForensicConfidence::Inconclusive;
        report.findings.push_back("File is corrupted, truncated, or format is unsupported");
        report.forensic_limitations.push_back("Cannot perform steganalysis on unparseable file");
        return report;
    }

    report.structurally_valid = true;
    report.detected_format = backend->format();
    report.format_description = backend->format_name();
    report.dimensions = backend->dimensions();
    report.raw_capacity_bytes = backend->calculate_raw_capacity_bytes();
    report.usable_payload_capacity_bytes = backend->calculate_usable_payload_capacity_bytes();

    // Check for container
    std::vector<uint8_t> extracted_container;
    std::string ext_err;
    if (backend->extract_container(extracted_container, ext_err)) {
        std::string hdr_err;
        auto hdr_opt = parse_container_header(extracted_container.data(), extracted_container.size(), hdr_err);
        if (hdr_opt) {
            report.container_detected = true;
            report.container_header = *hdr_opt;
            report.verdict = ForensicVerdict::ConfirmedRogueByteContainer;
            report.confidence = ForensicConfidence::ConfirmedContainer;

            std::ostringstream oss;
            oss << "Confirmed RogueByte Container v" << static_cast<int>(hdr_opt->version);
            oss << " (" << (hdr_opt->is_encrypted() ? "AES-256-GCM Encrypted" : "Plaintext");
            if (hdr_opt->is_multi_file()) oss << ", Multi-File Archive";
            oss << ", Payload: " << hdr_opt->payload_size << " bytes)";
            report.findings.push_back(oss.str());
            report.findings.push_back("Payload SHA-256: " + to_hex(hdr_opt->payload_sha256.data(), SHA256_DIGEST_LEN));
            if (hdr_opt->is_encrypted()) {
                report.findings.push_back("KDF: PBKDF2-HMAC-SHA256 (" + std::to_string(hdr_opt->kdf_iterations) + " iterations)");
                report.findings.push_back("Salt: " + to_hex(hdr_opt->salt.data(), hdr_opt->salt.size()));
                report.findings.push_back("Nonce: " + to_hex(hdr_opt->nonce.data(), hdr_opt->nonce.size()));
                report.findings.push_back("AEAD Tag: " + to_hex(hdr_opt->tag.data(), hdr_opt->tag.size()));
            }
        }
    }

    // Bitplane & statistical analysis for raster images
    const auto& pixels = backend->raw_pixel_data();
    if (!pixels.empty() && report.dimensions.width > 0 && report.dimensions.height > 0) {
        report.has_pixel_data = true;
        report.bitplanes = analyze_bitplanes(pixels, report.dimensions.width, report.dimensions.height);
        report.chi_square = perform_chi_square(pixels, report.dimensions.width, report.dimensions.height);

        if (!report.container_detected) {
            if (report.bitplanes.average_lsb_entropy > 0.995 && report.chi_square.statistically_anomalous) {
                report.verdict = ForensicVerdict::SuspectedStego;
                report.confidence = ForensicConfidence::HeuristicHigh;
                report.findings.push_back("High LSB Shannon Entropy (" +
                                          std::to_string(report.bitplanes.average_lsb_entropy) +
                                          ") combined with Chi-Square pair equalization indicates likely hidden pseudorandom payload");
            } else if (report.bitplanes.average_lsb_entropy > 0.985) {
                report.verdict = ForensicVerdict::SuspectedStego;
                report.confidence = ForensicConfidence::HeuristicMedium;
                report.findings.push_back("Elevated LSB entropy detected (" +
                                          std::to_string(report.bitplanes.average_lsb_entropy) +
                                          "); payload suspected but unverified");
            } else {
                report.verdict = ForensicVerdict::CleanOrInconclusive;
                report.confidence = ForensicConfidence::Inconclusive;
                report.findings.push_back("No RogueByte signatures found; LSB entropy falls within standard photographic baseline");
            }
        }
    } else {
        if (!report.container_detected) {
            report.verdict = ForensicVerdict::CleanOrInconclusive;
            report.confidence = ForensicConfidence::Inconclusive;
            report.findings.push_back("No known RogueByte steganography markers detected in image markers/segments");
        }
    }

    // Document limitations
    report.forensic_limitations.push_back("Absence of signatures does not prove absence of covert steganography.");
    report.forensic_limitations.push_back("Custom cryptographic keys, low-rate syndrome embedding, or non-LSB schemes cannot be ruled out.");
    report.forensic_limitations.push_back("Heuristic entropy metrics can yield false positives on highly textured or noisy images.");

    return report;
}

std::string ForensicReport::to_text_summary() const {
    std::ostringstream oss;
    oss << "================================================================================\n";
    oss << " ROGUEBYTE STEGO PRO — DIGITAL FORENSIC INSPECTION REPORT\n";
    oss << "================================================================================\n";
    oss << "Target File       : " << file_path.string() << "\n";
    oss << "File Size         : " << format_bytes(file_size) << " (" << file_size << " bytes)\n";
    oss << "SHA-256 Hash      : " << file_sha256 << "\n";
    oss << "Detected Format   : " << format_description << "\n";
    oss << "Dimensions        : " << dimensions.width << " x " << dimensions.height
        << " (" << dimensions.channels << " channels, " << dimensions.bit_depth << "-bit)\n";
    oss << "Raw Capacity      : " << format_bytes(raw_capacity_bytes) << "\n";
    oss << "Usable Payload    : " << format_bytes(usable_payload_capacity_bytes) << "\n";
    oss << "Structural Status : " << (structurally_valid ? "VALID" : "INVALID / CORRUPT") << "\n";

    if (has_pixel_data) {
        oss << "\n--- Bitplane Entropy Analysis (Shannon H) ---\n";
        oss << "Plane  |   Red    |  Green   |   Blue   |\n";
        oss << "-------+----------+----------+----------+\n";
        for (int b = 7; b >= 0; --b) {
            oss << "Bit " << b << (b == 0 ? " (LSB)" : "      ") << " | "
                << std::fixed << std::setprecision(4)
                << std::setw(8) << bitplanes.red_entropy[b] << " | "
                << std::setw(8) << bitplanes.green_entropy[b] << " | "
                << std::setw(8) << bitplanes.blue_entropy[b] << " |\n";
        }
        oss << "Average LSB Entropy: " << std::fixed << std::setprecision(5) << bitplanes.average_lsb_entropy << " / 1.00000\n";
        oss << "Chi-Square Metric  : " << std::fixed << std::setprecision(4) << chi_square.chi_square_stat
            << " (Normalized: " << chi_square.p_value << ")\n";
    }

    oss << "\n--- Forensic Findings ---\n";
    for (const auto& f : findings) {
        oss << "[+] " << f << "\n";
    }

    oss << "\n--- Verdict & Confidence ---\n";
    oss << "Final Verdict     : ";
    switch (verdict) {
        case ForensicVerdict::ConfirmedRogueByteContainer:
            oss << "CONFIRMED EMBEDDED CONTAINER\n";
            break;
        case ForensicVerdict::SuspectedStego:
            oss << "SUSPECTED STEGANOGRAPHY (HEURISTIC)\n";
            break;
        case ForensicVerdict::CleanOrInconclusive:
            oss << "CLEAN OR INCONCLUSIVE\n";
            break;
    }

    oss << "Confidence Level  : ";
    switch (confidence) {
        case ForensicConfidence::ConfirmedContainer: oss << "CONFIRMED (100% Signature Match)\n"; break;
        case ForensicConfidence::HeuristicHigh:      oss << "HEURISTIC HIGH\n"; break;
        case ForensicConfidence::HeuristicMedium:    oss << "HEURISTIC MEDIUM\n"; break;
        case ForensicConfidence::HeuristicLow:       oss << "HEURISTIC LOW\n"; break;
        case ForensicConfidence::Inconclusive:       oss << "INCONCLUSIVE / NO SIGNATURE\n"; break;
    }

    oss << "\n--- Forensic Limitations & Methodology ---\n";
    for (const auto& lim : forensic_limitations) {
        oss << "[!] " << lim << "\n";
    }
    oss << "================================================================================\n";

    return oss.str();
}

std::string ForensicReport::to_json() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"file_path\": \"" << file_path.string() << "\",\n";
    oss << "  \"file_size\": " << file_size << ",\n";
    oss << "  \"sha256\": \"" << file_sha256 << "\",\n";
    oss << "  \"format\": \"" << format_description << "\",\n";
    oss << "  \"width\": " << dimensions.width << ",\n";
    oss << "  \"height\": " << dimensions.height << ",\n";
    oss << "  \"channels\": " << dimensions.channels << ",\n";
    oss << "  \"raw_capacity_bytes\": " << raw_capacity_bytes << ",\n";
    oss << "  \"usable_payload_capacity_bytes\": " << usable_payload_capacity_bytes << ",\n";
    oss << "  \"container_detected\": " << (container_detected ? "true" : "false") << ",\n";

    if (container_detected) {
        oss << "  \"container\": {\n";
        oss << "    \"version\": " << static_cast<int>(container_header.version) << ",\n";
        oss << "    \"encrypted\": " << (container_header.is_encrypted() ? "true" : "false") << ",\n";
        oss << "    \"multi_file\": " << (container_header.is_multi_file() ? "true" : "false") << ",\n";
        oss << "    \"payload_size\": " << container_header.payload_size << ",\n";
        oss << "    \"payload_sha256\": \"" << to_hex(container_header.payload_sha256.data(), SHA256_DIGEST_LEN) << "\",\n";
        oss << "    \"kdf_iterations\": " << container_header.kdf_iterations << ",\n";
        oss << "    \"salt\": \"" << to_hex(container_header.salt.data(), container_header.salt.size()) << "\",\n";
        oss << "    \"nonce\": \"" << to_hex(container_header.nonce.data(), container_header.nonce.size()) << "\",\n";
        oss << "    \"tag\": \"" << to_hex(container_header.tag.data(), container_header.tag.size()) << "\"\n";
        oss << "  },\n";
    }

    oss << "  \"lsb_entropy\": " << bitplanes.average_lsb_entropy << ",\n";
    oss << "  \"chi_square\": " << chi_square.chi_square_stat << ",\n";
    oss << "  \"verdict\": \"" << (verdict == ForensicVerdict::ConfirmedRogueByteContainer ? "CONFIRMED" :
                                   verdict == ForensicVerdict::SuspectedStego ? "SUSPECTED" : "CLEAN_OR_INCONCLUSIVE") << "\",\n";
    oss << "  \"findings\": [\n";
    for (size_t i = 0; i < findings.size(); ++i) {
        oss << "    \"" << findings[i] << "\"" << (i + 1 < findings.size() ? "," : "") << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";
    return oss.str();
}

} // namespace roguebyte
