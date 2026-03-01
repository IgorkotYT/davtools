#include "common.hpp"
#include <filesystem>
#include <vector>
#include <string>

namespace fs = std::filesystem;

std::vector<OutputArtifact> convert_virustest(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    conv::TempDir tmp("conv-virustest-");
    const fs::path in_path = tmp.path() / (input_name.empty() ? "input.bin" : input_name);

    std::string out_name = conv::replace_extension(in_path.filename().string(), "png");
    if (out_name == in_path.filename().string()) {
        out_name += ".png";
    }
    const fs::path out_path = tmp.path() / out_name;

    conv::write_file_bytes(in_path, input);

    if (!conv::program_exists("clamscan")) {
        throw std::runtime_error("clamscan not found in PATH");
    }

    // Run clamscan.
    // We want the summary to make it look "official"
    auto scan_full = conv::run_process({"clamscan", in_path.string()});

    std::string report = scan_full.output;
    if (report.empty()) {
        report = "No output from clamscan. Exit code: " + std::to_string(scan_full.exit_code);
    }

    // Create PNG from text report using ImageMagick
    std::string magick = conv::detect_magick_cli();

    std::vector<std::string> magick_cmd;
    if (magick == "magick") {
        magick_cmd = {
            "magick",
            "-background", "white",
            "-fill", "black",
            "-pointsize", "14",
            "-border", "20",
            "-bordercolor", "white",
            "label:" + report,
            out_path.string()
        };
    } else {
        magick_cmd = {
            "convert",
            "-background", "white",
            "-fill", "black",
            "-pointsize", "14",
            "-border", "20",
            "-bordercolor", "white",
            "label:" + report,
            out_path.string()
        };
    }

    auto r = conv::run_process(magick_cmd);
    conv::require_success(r, magick);

    return { conv::make_artifact_from_file(out_path) };
}
