#ifndef ENCRYPTION_HPP
#define ENCRYPTION_HPP

#include <string>

// Простая функция XOR-«шифрования»
std::string xor_encrypt(const std::string &input, const std::string &key);

// Для расшифровки используется та же функция
inline std::string xor_decrypt(const std::string &input, const std::string &key) {
    return xor_encrypt(input, key); // XOR симметричен
}

#endif