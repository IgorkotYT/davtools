#include "common.hpp"

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

std::vector<OutputArtifact> convert_img_gif(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    conv::TempDir tmp("conv-img-gif-");

    std::string in_file = input_name.empty() ? "input.png" : input_name;
    if (conv::lower_ext(in_file).empty()) in_file += ".png";

    const fs::path in_path = tmp.path() / in_file;
    const fs::path out_path = tmp.path() / (conv::basename_no_ext(in_file) + ".gif");

    conv::write_file_bytes(in_path, input);

    std::vector<std::string> real;
    if (conv::program_exists("magick")) {
        real = {"magick", in_path.string(), out_path.string()};
    } else {
        real = {"convert", in_path.string(), out_path.string()};
    }

    auto r = conv::run_process(real);
    conv::require_success(r, real[0]);

    return { conv::make_artifact_from_file(out_path) };
}
