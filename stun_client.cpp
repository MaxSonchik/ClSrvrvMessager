#include "stun_client.h"
#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ip/udp.hpp>
#include <cstdint>

using namespace boost::asio;
using namespace boost::asio::ip;

StunClient::StunClient(const std::string &address, int port)
    : server_address(address), server_port(port) {}

bool StunClient::get_public_address(std::string &ip, int &port) {
    try {
        io_context io_context;
        udp::socket socket(io_context, udp::endpoint(udp::v4(), 0));
        udp::resolver resolver(io_context);
        udp::endpoint server_endpoint = *resolver.resolve(udp::v4(), server_address, std::to_string(server_port)).begin();

        // Формирование STUN Binding Request
        std::vector<uint8_t> request(20, 0x00);
        request[0] = 0x00; // Message Type (Binding Request)
        request[1] = 0x01; // Binding Request
        request[3] = 0x12; // Length (16)

        socket.send_to(buffer(request), server_endpoint);
        std::cout << "Sent STUN Binding Request to " << server_address << ":" << server_port << std::endl;

        // Получение ответа
        std::array<uint8_t, 1024> response;
        udp::endpoint sender_endpoint;
        size_t len = socket.receive_from(buffer(response), sender_endpoint);
        std::cout << "Received response of size: " << len << " bytes from " << sender_endpoint.address().to_string() << std::endl;

        // Парсинг ответа
        if (response[0] == 0x01 && response[1] == 0x01) { // Binding Response
            ip = std::to_string(response[12]) + "." + std::to_string(response[13]) + "." +
                 std::to_string(response[14]) + "." + std::to_string(response[15]);
            port = (response[10] << 8) | response[11];
            std::cout << "Parsed public IP: " << ip << ", Port: " << port << std::endl;
            return true;
        } else {
            std::cerr << "Invalid STUN response" << std::endl;
        }
    } catch (std::exception &e) {
        std::cerr << "Error in STUN client: " << e.what() << std::endl;
    }

    return false;
}
