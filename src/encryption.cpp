#include "encryption.hpp"

std::string xor_encrypt(const std::string &input, const std::string &key) {
    std::string out = input;
    for (size_t i = 0; i < out.size(); i++) {
        out[i] = out[i] ^ key[i % key.size()];
    }
    return out;
}

std::string xor_decrypt(const std::string &input, const std::string &key) { return xor_encrypt(input, key); }
