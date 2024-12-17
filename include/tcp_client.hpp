#ifndef TCP_CLIENT_HPP
#define TCP_CLIENT_HPP

#include <boost/asio.hpp>
#include "message.hpp"
#include "common.hpp"

class TCPClient {
public:
    TCPClient(const std::string &server_ip, uint16_t server_port);
    void connect();
    void register_client(const std::string &username, const PublicEndpoint &pub_ep);
    void send_message(const Message &msg);
    Message receive_message();

private:
    Message read_message();

    std::string server_ip_;
    uint16_t server_port_;
    boost::asio::io_context ioc_;
    boost::asio::ip::tcp::socket socket_;
};

#endif 
