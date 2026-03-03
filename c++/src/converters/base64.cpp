#include "common.hpp"

#include <openssl/evp.h>
#include <stdexcept>

std::vector<OutputArtifact> convert_base64(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    // EVP_EncodeBlock returns the number of bytes encoded, not including the NUL terminator.
    // It requires the output buffer to be large enough to hold 4 * ((input.size() + 2) / 3) bytes
    // plus one byte for the NUL terminator.
    size_t req_len = 4 * ((input.size() + 2) / 3);
    std::string encoded(req_len + 1, '\0');

    int ret = EVP_EncodeBlock(
        reinterpret_cast<unsigned char*>(encoded.data()),
        input.data(),
        input.size()
    );

    if (ret < 0) {
        throw std::runtime_error("EVP_EncodeBlock failed");
    }

    // Resize down to the actual encoded length returned by EVP_EncodeBlock
    encoded.resize(ret);
    encoded += "\n";

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.bin";
    out_name += ".b64.txt";

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "text/plain";
    artifact.data = encoded;

    return { artifact };
}
