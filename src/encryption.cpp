#include "encryption.hpp"

std::string xor_encrypt(const std::string &input, const std::string &key) {
    std::string output = input;
    for (size_t i = 0; i < output.size(); i++) {
        output[i] = output[i] ^ key[i % key.size()];
    }
    return output;
}
