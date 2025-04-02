#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <boost/asio.hpp>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

namespace tcp_messenger {

class TCPServer {
public:
    class Session;
    
    TCPServer(boost::asio::io_context& io_context, unsigned short port);
    void start();
    void stop();

private:
    void do_accept();
    void handle_message(const std::string& message, std::shared_ptr<Session> session);
    void remove_client(const std::string& username);

    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::unordered_map<std::string, std::shared_ptr<Session>> clients_;
    std::mutex clients_mutex_;
    std::unordered_map<std::string, unsigned short> udp_ports_;

    // Prometheus метрики
    prometheus::Exposer exposer_;
    std::shared_ptr<prometheus::Registry> registry_;
    prometheus::Counter& messages_total_;
    prometheus::Gauge& active_connections_;
};

} // namespace tcp_messenger

#endif