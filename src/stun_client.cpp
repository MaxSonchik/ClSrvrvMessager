#include "../include/stun_client.hpp"

#include <boost/asio.hpp>
#include <string>
#include <vector>

#include "../include/common.hpp"
#include "../include/stun.hpp"

StunClient::StunClient(const std::string &stun_server, uint16_t stun_port)
    : stun_server_(stun_server), stun_port_(stun_port) {}

PublicEndpoint StunClient::get_public_endpoint() {
    boost::asio::io_context ioc;
    boost::asio::ip::udp::socket socket(ioc);
    socket.open(boost::asio::ip::udp::v4());
    socket.non_blocking(false);

    boost::asio::ip::udp::resolver resolver(ioc);
    auto endpoints = resolver.resolve(stun_server_, std::to_string(stun_port_));
    auto endpoint = *endpoints.begin();

    auto request = build_stun_binding_request();
    boost::system::error_code ec;
    socket.send_to(boost::asio::buffer(request), endpoint, 0, ec);
    if (ec) {
        log_error("Failed to send STUN request: " + ec.message());
        // Возвращаем локальный адрес при ошибке
        PublicEndpoint local;
        local.ip = "127.0.0.1";
        local.port = 40000;
        return local;
    }

    std::vector<uint8_t> resp(1024);
    boost::asio::ip::udp::endpoint sender_ep;
    std::size_t len = socket.receive_from(boost::asio::buffer(resp), sender_ep, 0, ec);

    if (ec) {
        log_error("STUN receive_from failed: " + ec.message());
        PublicEndpoint local;
        local.ip = "127.0.0.1";
        local.port = 40000;
        return local;
    }

    resp.resize(len);
    std::string ip;
    uint16_t port = 0;
    if (parse_stun_binding_response(resp, ip, port)) {
        PublicEndpoint ep;
        ep.ip = ip;
        ep.port = port;
        return ep;
    }

    // При неудаче возвращаем локальный адрес
    PublicEndpoint local;
    local.ip = "127.0.0.1";
    local.port = 40000;
    return local;
}