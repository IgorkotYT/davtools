#include "common.hpp"

#include <openssl/evp.h>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<OutputArtifact> convert_base64(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    // Calculate expected length (Base64 is 4 chars for every 3 bytes, plus padding)
    // EVP_EncodeBlock adds a null terminator
    size_t expected_len = 4 * ((input.size() + 2) / 3) + 1;

    std::vector<unsigned char> out(expected_len);

    // EVP_EncodeBlock returns the number of bytes encoded, excluding the null terminator.
    int len = EVP_EncodeBlock(out.data(), input.data(), input.size());
    if (len < 0) {
        throw std::runtime_error("EVP_EncodeBlock failed");
    }

    std::string encoded(reinterpret_cast<char*>(out.data()), len);
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
