#ifndef ENCRYPTION_HPP
#define ENCRYPTION_HPP

#include <argon2.h>

#include <algorithm>
#include <random>
#include <string>
#include <vector>

namespace Security {
struct HashResult {
    std::string hash;
    std::string salt;
};

HashResult generate_hash(const std::string& password);
bool verify_password(const std::string& password, const std::string& stored_hash, const std::string& salt);
}  // namespace Security

#endif