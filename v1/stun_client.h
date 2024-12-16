#ifndef STUN_CLIENT_H
#define STUN_CLIENT_H

#include <string>
#include <vector>
#include <boost/asio.hpp>

class StunClient {
private:
    std::string server_address;
    int server_port;

public:
    StunClient(const std::string &address, int port);
    bool get_public_address(std::string &ip, int &port);
};

#endif // STUN_CLIENT_H
