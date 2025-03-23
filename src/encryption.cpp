#include "encryption.hpp"
#include <argon2.h>
#include <random>
#include <stdexcept>

namespace Security {

std::string generate_random_salt(size_t length) {
    static const char chars[] = 
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
    
    std::string salt;
    salt.reserve(length);
    for(size_t i = 0; i < length; ++i) {
        salt += chars[dis(gen)];
    }
    return salt;
}

HashResult generate_hash(const std::string& password, const std::string& salt) {
    HashResult result;
    result.salt = salt.empty() ? generate_random_salt(16) : salt;

    uint32_t t_cost = 2;
    uint32_t m_cost = 1 << 16;
    uint32_t parallelism = 1;
    result.hash.resize(32);

    int res = argon2_hash(t_cost, m_cost, parallelism,
                        password.data(),
                        password.size(),
                        result.salt.data(),
                        result.salt.size(),
                        result.hash.data(),
                        result.hash.size(),
                        nullptr,
                        0,
                        Argon2_id,
                        ARGON2_VERSION_NUMBER);

    if (res != ARGON2_OK) {
        throw std::runtime_error("Argon2 error: " + std::string(argon2_error_message(res)));
    }

    return result;
}

bool verify_password(const std::string& password, 
                    const std::string& stored_hash,
                    const std::string& stored_salt) {
    try {
        HashResult new_hash = generate_hash(password, stored_salt);
        return new_hash.hash == stored_hash;
    }
    catch (...) {
        return false;
    }
}

} // namespace Security