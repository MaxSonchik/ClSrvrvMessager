#include "tcp/tcp_server.hpp"
#include <boost/asio.hpp>
#include <iostream>

int main() {
    try {
        boost::asio::io_context io_context;
        tcp_messenger::TCPServer server(io_context, 12345);
        server.start();
        std::cout << "TCP Server started on port 12345\n";
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Server exception: " << e.what() << std::endl;
    }
    return 0;
}
