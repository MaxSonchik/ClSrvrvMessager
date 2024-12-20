#include "../include/udp_file_sender.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <fstream>
#include <thread>

#include "../include/common.hpp"
#include "../include/file_transfer_protocol.hpp"

UDPFileSender::UDPFileSender(boost::asio::io_context &ioc) : ioc_(ioc), socket_(ioc, boost::asio::ip::udp::v4()) {}

bool UDPFileSender::send_file(const std::string &file_path, const std::string &dest_ip, uint16_t dest_port) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        log_error("File not found: " + file_path);
        return false;
    }

    boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::make_address(dest_ip), dest_port);

    const size_t BLOCK_SIZE = 1024;
    std::vector<uint8_t> block(BLOCK_SIZE);
    uint32_t block_number = 0;

    while (true) {
        file.read((char *)block.data(), BLOCK_SIZE);
        std::streamsize bytes_read = file.gcount();
        if (bytes_read <= 0) break;
        block.resize((size_t)bytes_read);

        if (!send_block(block_number, block, endpoint)) return false;
        block_number++;
        block.resize(BLOCK_SIZE);
    }
    log_info("File sent successfully: " + file_path);
    return true;
}

bool UDPFileSender::send_block(uint32_t block_number, const std::vector<uint8_t> &block_data,
                               boost::asio::ip::udp::endpoint &endpoint) {
    FileDataPacket pkt;
    pkt.block_number = block_number;
    pkt.data = block_data;
    auto data = serialize_data_packet(pkt);

    int retries = 5;
    while (retries > 0) {
        boost::system::error_code ec;
        socket_.send_to(boost::asio::buffer(data), endpoint, 0, ec);
        if (wait_for_ack(block_number)) return true;
        retries--;
    }
    log_error("Failed to get ACK for block " + std::to_string(block_number));
    return false;
}

bool UDPFileSender::wait_for_ack(uint32_t block_number) {
    socket_.non_blocking(true);
    boost::system::error_code ec;
    boost::asio::ip::udp::endpoint remote_ep;

    for (int i = 0; i < 100; i++) {
        size_t len = socket_.receive_from(boost::asio::buffer(buffer_), remote_ep, 0, ec);
        if (!ec && len > 0) {
            FileAckPacket ack;
            std::vector<uint8_t> v(buffer_.begin(), buffer_.begin() + len);
            if (parse_ack_packet(v, ack)) {
                if (ack.block_number == block_number) {
                    return true;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}