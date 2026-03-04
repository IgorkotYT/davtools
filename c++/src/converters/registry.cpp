// registry.cpp
// not empty anymore
#include "registry.hpp"

#include <stdexcept>
#include <string_view>
#include <vector>

// Converter implementations (defined in their respective .cpp files)
std::vector<OutputArtifact> convert_png_jpg(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input);

std::vector<OutputArtifact> convert_invert(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input);

std::vector<OutputArtifact> convert_img_gif(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input);

std::vector<OutputArtifact> convert_pdf_png(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input);

std::vector<OutputArtifact> run_converter(
    std::string_view op,
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (op == "png-jpg") return convert_png_jpg(input_name, input);
    if (op == "invert")  return convert_invert(input_name, input);
    if (op == "img-gif") return convert_img_gif(input_name, input);
    if (op == "pdf-png") return convert_pdf_png(input_name, input);

    throw std::runtime_error("unsupported converter op: " + std::string(op));
}
