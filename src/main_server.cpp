#include <boost/asio.hpp>
#include <thread>

#include "../include/common.hpp"
#include "../include/tcp_server.hpp"
#include "../include/udp_file_server.hpp"  // Для обработки UDP

int main() {
    try {
        boost::asio::io_context tcp_ioc, udp_ioc;
        std::string db_path = "./data/database.db";

        // Запускаем сервер для TCP
        TCPServer tcp_server(tcp_ioc, SERVER_PORT, db_path);

        // Запускаем сервер для UDP
        UDPFileServer udp_file_server(udp_ioc, SERVER_PORT + 1);  // Порт для UDP-файлов

        // Запускаем оба сервера в разных потоках
        std::thread tcp_thread([&]() { tcp_server.start(); });

        std::thread udp_thread([&]() { udp_ioc.run(); });

        tcp_thread.join();
        udp_thread.join();
    } catch (std::exception &e) {
        log_error(e.what());
    }
    return 0;
}
