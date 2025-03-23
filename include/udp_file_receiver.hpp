#ifndef UDP_FILE_RECEIVER_HPP
#define UDP_FILE_RECEIVER_HPP

#include <boost/asio.hpp>
#include <array>

class UDPFileReceiver {
public:
    UDPFileReceiver(boost::asio::io_context& io_context, 
                  short port, 
                  const std::string& save_dir);
    void start();
    void stop(); // Добавляем объявление метода stop()

private:
    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint remote_endpoint_;
    static constexpr size_t buffer_size = 1024;
    std::array<char, buffer_size> receive_buffer_;
    short port_;
    std::string save_dir_;
    bool is_running_; // Добавляем флаг состояния
};

#endif