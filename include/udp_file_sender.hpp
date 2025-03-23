#ifndef UDP_FILE_SENDER_HPP
#define UDP_FILE_SENDER_HPP

#include <boost/asio.hpp>
#include <string>

class UDPFileSender {
public:
    UDPFileSender(boost::asio::io_context& io_context);
    ~UDPFileSender();

    bool send_file(const std::string& file_path, 
                 const std::string& ip, 
                 short port);
void stop();

private:
    static constexpr size_t CHUNK_SIZE = 508; // Максимальный размер UDP-пакета
    boost::asio::io_context& io_context_;
    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint receiver_endpoint_;
    bool is_running_;
};

#endif // UDP_FILE_SENDER_HPP