#include <openssl/evp.h>
#include <iostream>
#include <vector>
#include <string>

int main() {
    std::string input = "aGVsbG8gd29ybGQ=";
    int len = input.size();
    int out_len = 3 * len / 4;
    std::vector<unsigned char> out_buf(out_len);

    // Trim newlines if any

    int ret = EVP_DecodeBlock(out_buf.data(), (const unsigned char*)input.data(), len);
    if (ret < 0) {
        std::cerr << "decode error" << std::endl;
        return 1;
    }
    // padding?
    int pad = 0;
    if (len > 0 && input[len-1] == '=') pad++;
    if (len > 1 && input[len-2] == '=') pad++;

    std::string decoded(out_buf.begin(), out_buf.begin() + ret - pad);
    std::cout << decoded << std::endl;
    return 0;
}
