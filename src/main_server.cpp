#include "../include/common.hpp"
#include "../include/tcp_server.hpp"
#include <boost/asio.hpp>

int main() {
    try {
        boost::asio::io_context ioc;
        std::string db_path = "./data/database.db";
        TCPServer server(ioc, SERVER_PORT, db_path);
        server.start();
    } catch (std::exception &e) {
        log_error(e.what());
    }
    return 0;
}
