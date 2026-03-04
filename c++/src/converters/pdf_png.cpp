#include "../app.hpp"

#include <Magick++.h>

#include <iomanip>
#include <list>
#include <sstream>
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

std::vector<OutputArtifact> convert_pdf_png(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        throw std::runtime_error("empty input");
    }

    Magick::Blob in_blob(input.data(), input.size());

    // Multi-page PDF decode (requires Ghostscript + ImageMagick PDF policy enabled)
    std::list<Magick::Image> pages;
    Magick::readImages(&pages, in_blob);

    if (pages.empty()) {
        throw std::runtime_error("no pages decoded from PDF");
    }

    std::vector<OutputArtifact> out;
    const std::string stem = stem_of(input_name);

    int page_index = 1;
    for (auto& page : pages) {
        page.magick("PNG");
        page.strip();

        Magick::Blob out_blob;
        page.write(&out_blob);

        std::ostringstream name;
        name << stem << "_page_"
             << std::setw(4) << std::setfill('0') << page_index++
             << ".png";

        out.push_back(OutputArtifact{
            .name = name.str(),
            .content_type = "image/png",
            .data = blob_to_string(out_blob)
        });
    }

    return out;
}
