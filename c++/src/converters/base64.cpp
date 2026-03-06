#include "common.hpp"
#include <openssl/evp.h>
#include <vector>
#include <string>

std::vector<OutputArtifact> convert_base64(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        OutputArtifact artifact;
        artifact.name = input_name.empty() ? "input.bin.b64.txt" : input_name + ".b64.txt";
        artifact.content_type = "text/plain";
        artifact.data = "\n";
        return { artifact };
    }

    size_t expected_len = 4 * ((input.size() + 2) / 3);
    std::vector<unsigned char> out_buf(expected_len + 1);

    int out_len = EVP_EncodeBlock(out_buf.data(), input.data(), input.size());

    std::string encoded(reinterpret_cast<char*>(out_buf.data()), out_len);
    encoded += "\n";

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.bin";
    out_name += ".b64.txt";

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "text/plain";
    artifact.data = std::move(encoded);

    return { artifact };
}
