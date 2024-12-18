#ifndef COMMON_HPP
#define COMMON_HPP

#include <string>
#include <cstdint>

static const std::string STUN_SERVER = "stun.l.google.com";
static const uint16_t STUN_PORT = 19302;
static const uint16_t SERVER_PORT = 5000;

struct PublicEndpoint {
    std::string ip;
    uint16_t port;
};

void log_info(const std::string &msg);
void log_error(const std::string &msg);

#endif
