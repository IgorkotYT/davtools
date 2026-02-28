#include "common.hpp"

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

std::vector<OutputArtifact> convert_pdf_png(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    conv::TempDir tmp("conv-pdf-png-");

    std::string in_file = input_name.empty() ? "input.pdf" : input_name;
    if (conv::lower_ext(in_file) != "pdf") in_file = conv::basename_no_ext(in_file) + ".pdf";

    const fs::path in_path = tmp.path() / in_file;
    const std::string base = conv::basename_no_ext(in_file);

    conv::write_file_bytes(in_path, input);

    // %03d => multiple output files, one per page
    const fs::path pattern = tmp.path() / (base + "_page_%03d.png");

    std::vector<std::string> real;
    if (conv::program_exists("magick")) {
        real = {"magick", "-density", "150", in_path.string(), pattern.string()};
    } else {
        real = {"convert", "-density", "150", in_path.string(), pattern.string()};
    }

    auto r = conv::run_process(real);
    conv::require_success(r, real[0]);

    std::vector<OutputArtifact> out;
    for (const auto& p : conv::list_files_sorted(tmp.path())) {
        if (!p.has_extension() || conv::lower_ext(p.filename().string()) != "png") continue;
        if (p.filename().string().find(base + "_page_") != 0) continue;
        out.push_back(conv::make_artifact_from_file(p));
    }

    if (out.empty()) {
        throw std::runtime_error("pdf-png produced no PNG files (Ghostscript/ImageMagick policy?)");
    }

    return out;
}
