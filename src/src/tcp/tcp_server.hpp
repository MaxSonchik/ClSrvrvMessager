#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <boost/asio.hpp>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace tcp_messenger {

class TCPServer {
public:
    TCPServer(boost::asio::io_context& io_context, unsigned short port);
    void start();
    void stop();
    void remove_client(const std::string& username);

private:
    class Session; // Объявление вложенного класса
    void do_accept();
    void handle_message(const std::string& message, std::shared_ptr<Session> session);

    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;

    std::unordered_map<std::string, std::shared_ptr<Session>> clients_;
    std::unordered_map<std::string, unsigned short> udp_ports_;
    std::mutex clients_mutex_;
};

} // namespace tcp_messenger

#endif