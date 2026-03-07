#include "common.hpp"

#include <openssl/evp.h>
#include <string>
#include <vector>
#include <stdexcept>

std::vector<OutputArtifact> convert_base64_encode(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    // The maximum length of base64 encoding is 4 * ((N + 2) / 3) + 1 for null terminator.
    // We add a bit of padding just to be safe.
    std::size_t out_len = 4 * ((input.size() + 2) / 3) + 1;

    // We also want a trailing newline to match standard base64 utilities nicely.
    std::vector<char> encoded(out_len + 1);

    int encoded_len = EVP_EncodeBlock(
        reinterpret_cast<unsigned char*>(encoded.data()),
        input.data(),
        input.size()
    );

    if (encoded_len < 0) {
        throw std::runtime_error("EVP_EncodeBlock failed");
    }

    std::string result(encoded.data(), encoded_len);
    result += "\n";

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.bin";
    out_name += ".base64.txt";

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "text/plain";
    artifact.data = result;

    return { artifact };
}
