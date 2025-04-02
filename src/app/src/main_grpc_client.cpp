#include "grpc/grpc_client.hpp"
#include <iostream>

int main() {
    std::string username, server_addr;
    std::cout << "Enter username: ";
    std::cin >> username;
    std::cout << "Enter server address (e.g., 127.0.0.1:50051): ";
    std::cin >> server_addr;

    grpc_messenger::GrpcClient client(username, server_addr);
    client.run();

    return 0;
}
