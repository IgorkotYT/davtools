#include "../app.hpp"

#include <Magick++.h>

#include <stdexcept>
#include <string>
#include <vector>

static std::string stem_of(const std::string& name) {
    auto slash = name.find_last_of("/\\");
    std::string base = (slash == std::string::npos) ? name : name.substr(slash + 1);
    auto dot = base.find_last_of('.');
    if (dot == std::string::npos) return base;
    return base.substr(0, dot);
}

static std::string blob_to_string(const Magick::Blob& b) {
    return std::string(static_cast<const char*>(b.data()), b.length());
}

static Magick::Image read_image_from_bytes(const std::vector<std::uint8_t>& input) {
    if (input.empty()) {
        throw std::runtime_error("empty input");
    }

    Magick::Blob in_blob(input.data(), input.size());
    Magick::Image img;
    img.read(in_blob);
    return img;
}

std::vector<OutputArtifact> convert_img_gif(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    Magick::Image img = read_image_from_bytes(input);

    // Single-frame GIF output
    img.magick("GIF");
    img.strip();

    Magick::Blob out_blob;
    img.write(&out_blob);

    OutputArtifact out{
        .name = stem_of(input_name) + ".gif",
        .content_type = "image/gif",
        .data = blob_to_string(out_blob)
    };

    return { std::move(out) };
}
