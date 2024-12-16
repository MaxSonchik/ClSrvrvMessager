#ifndef UDP_FILE_RECEIVER_HPP
#define UDP_FILE_RECEIVER_HPP

#include <string>
#include <boost/asio.hpp>
#include <fstream>

class UDPFileReceiver {
public:
    UDPFileReceiver(boost::asio::io_context &ioc, uint16_t port, const std::string &save_path);
    void start();

private:
    void do_receive();
    void send_ack(uint32_t block_number, const boost::asio::ip::udp::endpoint &sender_ep);

    boost::asio::io_context &ioc_;
    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint sender_ep_;
    std::string save_path_;
    std::ofstream ofs_;
    uint32_t expected_block_ = 0;
    std::array<uint8_t, 2048> buffer_;
};

#endif