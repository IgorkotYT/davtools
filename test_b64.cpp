#include <iostream>
#include <vector>
#include <string>
#include <openssl/evp.h>

int main() {
    std::string in_str = "hello world";
    std::vector<uint8_t> in(in_str.begin(), in_str.end());
    size_t req_len = 4 * ((in.size() + 2) / 3);
    // string size + 1 to accommodate NUL byte
    std::string out(req_len + 1, '\0');
    int ret = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(out.data()), in.data(), in.size());
    out.resize(ret);
    std::cout << "Encoded: " << out << " length: " << ret << std::endl;
    return 0;
}
