#include <boost/asio.hpp>
#include <iostream>
#include <thread>

using boost::asio::ip::tcp;
using boost::asio::ip::udp;

class Client {
   public:
    Client(boost::asio::io_service& io_service, const std::string& host, short tcp_port, short udp_port)
        : tcp_socket_(io_service),
          udp_socket_(io_service),
          udp_endpoint_(boost::asio::ip::address::from_string(host), udp_port) {
        // Устанавливаем соединение по TCP
        tcp_endpoint_ = tcp::endpoint(boost::asio::ip::address::from_string(host), tcp_port);
    }

    void start() {
        start_tcp_connection();
        start_udp_send_receive();
    }

   private:
    // TCP
    void start_tcp_connection() {
        tcp_socket_.connect(tcp_endpoint_);
        std::string message;
        boost::asio::read(tcp_socket_, boost::asio::buffer(message));
        std::cout << "Received from TCP server: " << message << std::endl;
    }

    // UDP
    void start_udp_send_receive() {
        std::string message = "Hello from UDP client!";
        udp_socket_.send_to(boost::asio::buffer(message), udp_endpoint_);

        char recv_buffer[1024];
        udp::endpoint sender_endpoint;
        size_t len = udp_socket_.receive_from(boost::asio::buffer(recv_buffer), sender_endpoint);
        std::cout << "Received from UDP server: " << std::string(recv_buffer, len) << std::endl;
    }

    tcp::socket tcp_socket_;
    udp::socket udp_socket_;
    tcp::endpoint tcp_endpoint_;
    udp::endpoint udp_endpoint_;
};

int main() {
    try {
        boost::asio::io_service io_service;
        Client client(io_service, "94.23.173.241", 12345, 12346);  // IP сервера и порты
        client.start();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}