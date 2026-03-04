#include <openssl/evp.h>
#include <iostream>
#include <vector>
#include <string>

int main() {
    std::string input = "hello world";
    int out_len = 4 * ((input.size() + 2) / 3);
    std::vector<unsigned char> out_buf(out_len + 1);
    int ret = EVP_EncodeBlock(out_buf.data(), (const unsigned char*)input.data(), input.size());
    std::string encoded(out_buf.begin(), out_buf.begin() + ret);
    std::cout << encoded << std::endl;
    return 0;
}
