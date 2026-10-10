#pragma once

#include "roguebyte_stego/image_backend.hpp"
#include <string>
#include <vector>
#include <array>
#include <optional>

namespace roguebyte {

enum class ForensicConfidence {
    Inconclusive,
    HeuristicLow,
    HeuristicMedium,
    HeuristicHigh,
    ConfirmedContainer
};

enum class ForensicVerdict {
    CleanOrInconclusive,
    SuspectedStego,
    ConfirmedRogueByteContainer
};

struct BitplaneAnalysis {
    double red_entropy[8]{0.0};
    double green_entropy[8]{0.0};
    double blue_entropy[8]{0.0};
    double average_lsb_entropy{0.0};
};

struct ChiSquareAnalysis {
    double chi_square_stat{0.0};
    double degrees_of_freedom{0.0};
    double p_value{0.0};
    bool statistically_anomalous{false};
};

struct ForensicReport {
    std::filesystem::path file_path;
    std::string file_sha256;
    uint64_t file_size{0};
    ImageFormat detected_format{ImageFormat::Unknown};
    std::string format_description;
    bool structurally_valid{false};
    std::string structural_notes;

    ImageDimensions dimensions{};
    size_t raw_capacity_bytes{0};
    size_t usable_payload_capacity_bytes{0};

    // Bitplane & statistical metrics
    bool has_pixel_data{false};
    BitplaneAnalysis bitplanes{};
    ChiSquareAnalysis chi_square{};

    // RogueByte container metadata (if found)
    bool container_detected{false};
    ContainerHeader container_header{};

    // Final assessment
    ForensicVerdict verdict{ForensicVerdict::CleanOrInconclusive};
    ForensicConfidence confidence{ForensicConfidence::Inconclusive};
    std::vector<std::string> findings;
    std::vector<std::string> forensic_limitations;

    // Formatting helpers
    std::string to_text_summary() const;
    std::string to_json() const;
};

// Main forensic analysis function
ForensicReport inspect_image(const std::filesystem::path& path);

} // namespace roguebyte
