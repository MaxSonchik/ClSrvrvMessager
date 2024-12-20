#include "../include/udp_file_receiver.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <fstream>
#include <thread>

#include "../include/common.hpp"
#include "../include/file_transfer_protocol.hpp"

UDPFileReceiver::UDPFileReceiver(boost::asio::io_context &ioc, uint16_t port, const std::string &save_path)
    : ioc_(ioc),
      socket_(ioc, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)),
      save_path_(save_path),
      expected_block_(0) {}

void UDPFileReceiver::start() {
    do_receive();
    ioc_.run();
}

void UDPFileReceiver::do_receive() {
    socket_.async_receive_from(boost::asio::buffer(buffer_.data(), buffer_.size()), sender_ep_, 0,
                               [this](const boost::system::error_code &ec, std::size_t bytes_received) {
                                   if (!ec && bytes_received > 0) {
                                       std::vector<uint8_t> v(buffer_.begin(), buffer_.begin() + bytes_received);
                                       FileDataPacket dpkt;

                                       if (parse_data_packet(v, dpkt)) {
                                           if (dpkt.block_number == 0 && !ofs_.is_open()) {
                                               // Генерируем имя файла на основе данных
                                               std::string filename =
                                                   "received_file_" + std::to_string(std::time(nullptr)) + ".bin";
                                               std::string file_path = save_path_ + "/" + filename;

                                               ofs_.open(file_path, std::ios::binary);
                                               if (!ofs_) {
                                                   log_error("Cannot open file for writing: " + file_path);
                                                   return;
                                               }
                                               log_info("Started receiving file: " + file_path);
                                           }

                                           if (dpkt.block_number == expected_block_) {
                                               ofs_.write((char *)dpkt.data.data(), dpkt.data.size());
                                               send_ack(dpkt.block_number, sender_ep_);
                                               expected_block_++;
                                           } else {
                                               send_ack(expected_block_ - 1, sender_ep_);
                                           }
                                       }
                                   }
                                   do_receive();
                               });
}

void UDPFileReceiver::send_ack(uint32_t block_number, const boost::asio::ip::udp::endpoint &sender_ep) {
    FileAckPacket ack;
    ack.block_number = block_number;
    auto data = serialize_ack_packet(ack);
    boost::system::error_code ec;
    socket_.send_to(boost::asio::buffer(data), sender_ep, 0, ec);
    if (ec) {
        log_error("Failed to send ACK: " + ec.message());
    }
}
