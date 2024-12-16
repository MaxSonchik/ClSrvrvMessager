#ifndef STUN_CLIENT_HPP
#define STUN_CLIENT_HPP

#include <string>
#include "common.hpp"

// STUN-клиент для получения публичного endpoint
class StunClient {
public:
    StunClient(const std::string &stun_server, uint16_t stun_port);
    PublicEndpoint get_public_endpoint();

private:
    std::string stun_server_;
    uint16_t stun_port_;
};

#endif
