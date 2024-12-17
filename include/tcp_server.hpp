#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <boost/asio.hpp>
#include <memory>
#include <unordered_map>
#include <string>
#include "message.hpp"
#include "common.hpp"
#include "database.hpp"

class TCPServer {
public:
    TCPServer(boost::asio::io_context &ioc, uint16_t port, const std::string &db_path);
    void start();

private:
    void do_accept();
    void handle_client(std::shared_ptr<boost::asio::ip::tcp::socket> socket);

    Database db_;
    std::unordered_map<std::string, PublicEndpoint> clients_;

    boost::asio::io_context &ioc_;
    boost::asio::ip::tcp::acceptor acceptor_;
};

#endif
