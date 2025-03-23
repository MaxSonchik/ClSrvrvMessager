#ifndef ENCRYPTION_HPP
#define ENCRYPTION_HPP

#include <string>

namespace Security {
    struct HashResult {
        std::string hash;
        std::string salt;
    };

    std::string generate_random_salt(size_t length);
    HashResult generate_hash(const std::string& password, const std::string& salt = "");
    bool verify_password(const std::string& password, 
                        const std::string& stored_hash,
                        const std::string& stored_salt);
}

#endif