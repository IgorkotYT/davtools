#include "common.hpp"

#include <openssl/evp.h>
#include <iomanip>
#include <sstream>

std::vector<OutputArtifact> convert_sha256(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        throw std::runtime_error("EVP_MD_CTX_new failed");
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestInit_ex failed");
    }

    if (EVP_DigestUpdate(ctx, input.data(), input.size()) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestUpdate failed");
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int lengthOfHash = 0;

    if (EVP_DigestFinal_ex(ctx, hash, &lengthOfHash) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestFinal_ex failed");
    }

    EVP_MD_CTX_free(ctx);

    std::stringstream ss;
    for (unsigned int i = 0; i < lengthOfHash; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    std::string digest = ss.str() + "\n";

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.bin";
    out_name += ".sha256.txt";

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "text/plain";
    artifact.data = digest;

    return { artifact };
}
