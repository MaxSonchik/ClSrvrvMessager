#ifndef ENCRYPTION_HPP
#define ENCRYPTION_HPP

#include <string>

std::string xor_encrypt(const std::string &input, const std::string &key);
std::string xor_decrypt(const std::string &input, const std::string &key);

#endif