#include "tcp/tcp_client.hpp"
#include <iostream>

int main() {
    std::string username, host;
    unsigned short port;

    std::cout << "Enter username: ";
    std::cin >> username;
    std::cout << "Enter server IP (e.g., 127.0.0.1): ";
    std::cin >> host;
    std::cout << "Enter port (e.g., 12345): ";
    std::cin >> port;

    tcp_messenger::TCPClient client(username, host, port);
    client.run();

    return 0;
}
