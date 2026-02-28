#include "common.hpp"

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

std::vector<OutputArtifact> convert_png_jpg(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    conv::TempDir tmp("conv-png-jpg-");
    const fs::path in_path  = tmp.path() / (input_name.empty() ? "input.png" : input_name);
    const fs::path out_path = tmp.path() / conv::replace_extension(in_path.filename().string(), "jpg");

    conv::write_file_bytes(in_path, input);

    // Flatten alpha to white to avoid black transparent areas.
    auto args = conv::magick_args({
        "convert",
        in_path.string(),
        "-background", "white",
        "-alpha", "remove",
        "-alpha", "off",
        "-quality", "90",
        out_path.string()
    });
    if (!args.empty() && args[0] == "convert") args.erase(args.begin()); // convert-mode path
    std::vector<std::string> real;
    if (conv::program_exists("magick")) {
        real = {"magick", in_path.string(), "-background", "white", "-alpha", "remove", "-alpha", "off", "-quality", "90", out_path.string()};
    } else {
        real = {"convert", in_path.string(), "-background", "white", "-alpha", "remove", "-alpha", "off", "-quality", "90", out_path.string()};
    }

    auto r = conv::run_process(real);
    conv::require_success(r, real[0]);

    return { conv::make_artifact_from_file(out_path) };
}
