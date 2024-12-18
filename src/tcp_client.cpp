#include "../include/tcp_client.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <array>
#include "../include/common.hpp"

using boost::asio::ip::tcp;

TCPClient::TCPClient(const std::string &server_ip, uint16_t server_port)
: server_ip_(server_ip), server_port_(server_port), socket_(ioc_) {}

void TCPClient::connect() {
    tcp::resolver resolver(ioc_);
    auto endpoints = resolver.resolve(server_ip_, std::to_string(server_port_));
    boost::asio::connect(socket_, endpoints);
    log_info("Connected to server");
}

void TCPClient::send_message(const Message &msg) {
    auto data = serialize_message(msg);
    boost::asio::write(socket_, boost::asio::buffer(data));
}

Message TCPClient::receive_message() {
    return read_message();
}

Message TCPClient::read_message() {
    std::array<uint8_t,4> length_bytes;
    boost::asio::read(socket_, boost::asio::buffer(length_bytes));
    uint32_t length = length_bytes[0] | (length_bytes[1]<<8) | (length_bytes[2]<<16) | (length_bytes[3]<<24);
    std::vector<uint8_t> buf(length);
    boost::asio::read(socket_, boost::asio::buffer(buf));
    return deserialize_message(buf);
}
