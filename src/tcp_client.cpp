#include "../include/tcp_client.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <array>
#include "message.hpp" 
#include "../include/common.hpp"


using boost::asio::ip::tcp;

TCPClient::TCPClient(const std::string &server_ip, uint16_t server_port)
    : server_ip_(server_ip), server_port_(server_port), socket_(ioc_) {}



TCPClient::~TCPClient() {
    if (socket_.is_open()) {
        boost::system::error_code ec;
        socket_.close(ec);
        if (ec) {
            std::cerr << "Error closing client socket: " << ec.message() << std::endl;
        }
    }
}



void TCPClient::connect() {
    try {
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(server_ip_), server_port_);
        socket_.connect(endpoint);
        std::cout << "Client connected to " << server_ip_ << ":" << server_port_ << std::endl;
    } catch (const boost::system::system_error &e) {
        throw std::runtime_error("Connection failed: " + std::string(e.what()));
    }
}


void TCPClient::register_client(const std::string &username, const std::string &password) {
    Message msg;
    msg.type = MessageType::ClientRegistration;
    msg.sender = username;
    msg.password = password;
    msg.text = "Registering";
    send_message(msg);
}



void TCPClient::send_message(const Message &msg) {
    if (!socket_.is_open()) {
        throw std::runtime_error("Socket is not open. Cannot send message.");
    }

    try {
        auto data = serialize_message(msg);
        boost::asio::write(socket_, boost::asio::buffer(data));
    } catch (const boost::system::system_error &e) {
        throw std::runtime_error("Error sending message: " + std::string(e.what()));
    }
}



Message TCPClient::receive_message() {
    try {
        std::vector<uint8_t> length_buf(4);
        boost::asio::read(socket_, boost::asio::buffer(length_buf));

        uint32_t msg_length = (length_buf[0]) | (length_buf[1] << 8) | (length_buf[2] << 16) | (length_buf[3] << 24);
        std::vector<uint8_t> data(msg_length);
        boost::asio::read(socket_, boost::asio::buffer(data));

        return deserialize_message(data);
    } catch (const boost::system::system_error &e) {
        log_error("Error receiving message: " + std::string(e.what()));
        throw;
    }
}



Message TCPClient::read_message() {
    std::array<uint8_t,4> length_bytes;
    boost::asio::read(socket_, boost::asio::buffer(length_bytes));
    uint32_t length = length_bytes[0] | (length_bytes[1]<<8) | (length_bytes[2]<<16) | (length_bytes[3]<<24);
    std::vector<uint8_t> buf(length);
    boost::asio::read(socket_, boost::asio::buffer(buf));
    return deserialize_message(buf);
}
