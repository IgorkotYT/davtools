#include "common.hpp"

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

std::vector<OutputArtifact> convert_mp4_gif(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (!conv::program_exists("ffmpeg")) {
        throw std::runtime_error("ffmpeg not found in PATH");
    }

    conv::TempDir tmp("conv-mp4-gif-");

    std::string in_file = input_name.empty() ? "input.mp4" : input_name;
    if (conv::lower_ext(in_file).empty()) in_file += ".mp4";

    const fs::path in_path = tmp.path() / in_file;
    const fs::path out_path = tmp.path() / (conv::basename_no_ext(in_file) + ".gif");

    conv::write_file_bytes(in_path, input);

    // Simple, robust conversion
    auto r = conv::run_process({
        "ffmpeg",
        "-y",
        "-v", "error",
        "-i", in_path.string(),
        "-vf", "fps=12,scale=iw:-1:flags=lanczos",
        out_path.string()
    });

    conv::require_success(r, "ffmpeg");

    return { conv::make_artifact_from_file(out_path) };
}
