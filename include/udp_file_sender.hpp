#ifndef UDP_FILE_SENDER_HPP
#define UDP_FILE_SENDER_HPP

#include <string>
#include <boost/asio.hpp>
#include <fstream>
#include <thread>
#include <chrono>

// Отправка файла по UDP с подтверждениями
class UDPFileSender {
public:
    UDPFileSender(boost::asio::io_context &ioc);
    // Возвращает true если успешно
    bool send_file(const std::string &file_path, const std::string &dest_ip, uint16_t dest_port);

private:
    bool send_block(uint32_t block_number, const std::vector<uint8_t> &block_data, boost::asio::ip::udp::endpoint &endpoint);
    bool wait_for_ack(uint32_t block_number);

    boost::asio::io_context &ioc_;
    boost::asio::ip::udp::socket socket_;
    std::array<uint8_t, 2048> buffer_;
};

#endif
