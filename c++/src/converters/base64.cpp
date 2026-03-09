#include "common.hpp"

#include <openssl/evp.h>
#include <string>

std::vector<OutputArtifact> convert_base64(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        std::string out_name = input_name.empty() ? "input.bin.b64.txt" : input_name + ".b64.txt";
        OutputArtifact artifact;
        artifact.name = out_name;
        artifact.content_type = "text/plain";
        artifact.data = "\n";
        return { artifact };
    }

    // Calculate required output size: 4 * ((len + 2) / 3) + 1 for null terminator
    std::size_t required_len = 4 * ((input.size() + 2) / 3) + 1;
    std::vector<unsigned char> out_buf(required_len);

    int encoded_len = EVP_EncodeBlock(out_buf.data(), input.data(), input.size());
    if (encoded_len < 0) {
        throw std::runtime_error("EVP_EncodeBlock failed");
    }

    std::string base64_str(reinterpret_cast<char*>(out_buf.data()), encoded_len);
    base64_str += "\n";

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.bin";
    out_name += ".b64.txt";

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "text/plain";
    artifact.data = base64_str;

    return { artifact };
}
