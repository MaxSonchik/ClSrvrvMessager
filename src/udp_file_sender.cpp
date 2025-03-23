#include "udp_file_sender.hpp"
#include <fstream>
#include <stdexcept>

UDPFileSender::UDPFileSender(boost::asio::io_context& io_context)
    : io_context_(io_context),
      socket_(io_context, boost::asio::ip::udp::v4()),
      is_running_(true) {}

UDPFileSender::~UDPFileSender() { stop(); }

bool UDPFileSender::send_file(const std::string& file_path, const std::string& ip, short port) {
    if (!is_running_) {
        throw std::runtime_error("UDPFileSender is not running");
    }

    try {
        std::ifstream file(file_path, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("Failed to open file: " + file_path);

        receiver_endpoint_ = boost::asio::ip::udp::endpoint(
            boost::asio::ip::make_address(ip), port);

        // Читаем файл по частям
        std::vector<char> buffer(CHUNK_SIZE);
        while (file.read(buffer.data(), CHUNK_SIZE)) {
            socket_.send_to(boost::asio::buffer(buffer.data(), file.gcount()), receiver_endpoint_);
        }

        // Отправляем последний пакет
        if (file.gcount() > 0) {
            socket_.send_to(boost::asio::buffer(buffer.data(), file.gcount()), receiver_endpoint_);
        }

        return true;
    } catch (const std::exception& e) {
        stop();
        throw std::runtime_error("Failed to send file: " + std::string(e.what()));
    }
}

void UDPFileSender::stop() {
    if (is_running_) {
        is_running_ = false;
        socket_.close();
    }
}