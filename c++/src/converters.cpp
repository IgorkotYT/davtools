#include "app.hpp"

#include <Magick++.h>
#include <stdexcept>

std::string convert_png_to_jpg(const std::vector<std::uint8_t>& input) {
    if (input.empty()) {
        throw std::runtime_error("empty input");
    }

    Magick::Blob in_blob(input.data(), input.size());

    Magick::Image img;
    img.read(in_blob); // auto-detect from input bytes

    // JPEG has no alpha - flatten to white to avoid black transparent areas
    img.backgroundColor(Magick::Color("white"));
    img.alpha(false);

    // Force regular color JPEG
    img.colorSpace(Magick::sRGBColorspace);
    img.type(Magick::TrueColorType);

    img.magick("JPEG");
    img.quality(90);
    img.strip();

    Magick::Blob out_blob;
    img.write(&out_blob);

    return std::string(
        static_cast<const char*>(out_blob.data()),
        out_blob.length()
    );
}
