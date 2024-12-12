#ifndef STUN_UTILS_H
#define STUN_UTILS_H

#include <vector>
#include <string>
#include <cstdint> // Для uint8_t

class StunUtils {
public:
    static std::vector<uint8_t> create_binding_request();
    static void parse_binding_response(const std::vector<uint8_t>& response, std::string& ip, int& port);
};

#endif // STUN_UTILS_H
