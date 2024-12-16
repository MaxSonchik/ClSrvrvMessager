#include "../include/udp_file_receiver.hpp"
#include "../include/file_transfer_protocol.hpp"
#include "../include/common.hpp"

#include <thread>
#include <chrono>
#include <fstream>
#include <boost/asio.hpp>

UDPFileReceiver::UDPFileReceiver(boost::asio::io_context &ioc, uint16_t port, const std::string &save_path)
: ioc_(ioc),
  socket_(ioc, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)),
  save_path_(save_path)
{
    ofs_.open(save_path_, std::ios::binary);
    if(!ofs_) {
        log_error("Cannot open file for writing: " + save_path_);
    }
}

void UDPFileReceiver::start() {
    do_receive();
    ioc_.run();
}

void UDPFileReceiver::do_receive() {
    socket_.async_receive_from(
        boost::asio::buffer(buffer_.data(), buffer_.size()),
        sender_ep_,
        0, // Передаем флаги (для UDP обычно 0)
        [this](const boost::system::error_code &ec, std::size_t bytes_received) {
            if(!ec && bytes_received > 0) {
                std::vector<uint8_t> v(buffer_.begin(), buffer_.begin() + bytes_received);
                FileDataPacket dpkt;
                if (parse_data_packet(v, dpkt)) {
                    if (dpkt.block_number == expected_block_) {
                        ofs_.write((char*)dpkt.data.data(), dpkt.data.size());
                        send_ack(dpkt.block_number, sender_ep_);
                        expected_block_++;
                    } else {
                        // Повторим ACK последнего корректного блока
                        send_ack(expected_block_-1, sender_ep_);
                    }
                }
            }
            do_receive();
        }
    );
}

void UDPFileReceiver::send_ack(uint32_t block_number, const boost::asio::ip::udp::endpoint &sender_ep) {
    FileAckPacket ack;
    ack.block_number = block_number;
    auto data = serialize_ack_packet(ack);
    boost::system::error_code ec;
    socket_.send_to(boost::asio::buffer(data), sender_ep, 0, ec);
    if(ec) {
        log_error("Failed to send ACK: " + ec.message());
    }
}