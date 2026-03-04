#include "common.hpp"

#include <openssl/evp.h>
#include <string>

std::vector<OutputArtifact> convert_base64(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    // Handle empty input gracefully
    if (input.empty()) {
        OutputArtifact artifact;
        artifact.name = input_name.empty() ? "input.b64.txt" : input_name + ".b64.txt";
        artifact.content_type = "text/plain";
        artifact.data = "";
        return { artifact };
    }

    int out_len = 4 * ((input.size() + 2) / 3);
    std::vector<unsigned char> out_buf(out_len + 1); // +1 for null terminator just in case, though EVP_EncodeBlock handles it

    int ret = EVP_EncodeBlock(out_buf.data(), input.data(), input.size());

    // EVP_EncodeBlock returns the number of bytes encoded, excluding the NUL terminator
    std::string encoded(out_buf.begin(), out_buf.begin() + ret);

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.bin";
    out_name += ".b64.txt";

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "text/plain";
    artifact.data = encoded + "\n"; // Adding a newline for convenience like sha256

    return { artifact };
}
