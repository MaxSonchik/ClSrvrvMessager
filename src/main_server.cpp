#include "../include/common.hpp"
#include "../include/tcp_server.hpp"
#include <boost/asio.hpp>

int main() {
    try {
        boost::asio::io_context ioc;
        TCPServer server(ioc, SERVER_PORT);
        server.start();
    } catch(std::exception &e) {
        log_error(e.what());
    }
    return 0;
}
