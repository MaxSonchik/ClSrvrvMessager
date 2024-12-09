#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <thread>

//ЛИНТЕРЫ
//find . -name "*.cpp"| xargs clang-format -i
//добавил в мейкфайл, для форматирования можно просто прописать make clang
//при использовании заголовочных файлов юзаем 
//find . -name "*.cpp" -o -name | xargs clang-format -n


using boost::asio::ip::tcp;
using boost::asio::ip::udp;

class Server {
   public:
    Server(boost::asio::io_context& io_context, short tcp_port, short udp_port)
        : tcp_acceptor_(io_context, tcp::endpoint(tcp::v4(), tcp_port)),
          udp_socket_(io_context, udp::endpoint(udp::v4(), udp_port)) {}

    void start() {
        start_tcp_accept();
        start_udp_receive();
    }

   private:
    // TCP
    void start_tcp_accept() {
        tcp_socket_ = std::make_shared<tcp::socket>(tcp_acceptor_.get_executor());
        tcp_acceptor_.async_accept(*tcp_socket_, [this](boost::system::error_code ec) {
            if (!ec) {
                handle_tcp_request();
            }
            start_tcp_accept();
        });
    }

    void handle_tcp_request() {
        std::string message = "Hello from TCP server!";
        boost::asio::write(*tcp_socket_, boost::asio::buffer(message));
    }

    // UDP
    void start_udp_receive() {
        udp_socket_.async_receive_from(boost::asio::buffer(udp_buffer_), udp_endpoint_,
                                       [this](boost::system::error_code ec, std::size_t bytes_recvd) {
                                           if (!ec && bytes_recvd > 0) {
                                               handle_udp_request(bytes_recvd);
                                           }
                                           start_udp_receive();
                                       });
    }

    void handle_udp_request([[maybe_unused]] std::size_t length) {
        std::string message = "Hello from UDP server!";
        udp_socket_.send_to(boost::asio::buffer(message), udp_endpoint_);
    }

    tcp::acceptor tcp_acceptor_;
    std::shared_ptr<tcp::socket> tcp_socket_;
    udp::socket udp_socket_;
    udp::endpoint udp_endpoint_;
    char udp_buffer_[1024];
};

int main() {
    try {
        boost::asio::io_context io_context;
        Server server(io_context, 12345, 12346);
        server.start();
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}
