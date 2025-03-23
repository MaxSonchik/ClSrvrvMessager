#include "../include/udp_file_receiver.hpp"
#include <boost/asio.hpp>
#include <fstream>

using namespace boost::asio;
using namespace boost::asio::ip;

UDPFileReceiver::UDPFileReceiver(io_context& io_context,
                               short port,
                               const std::string& save_dir)
    : socket_(io_context, udp::v4()),
      remote_endpoint_(),
      receive_buffer_{},
      port_(port),
      save_dir_(save_dir),
      is_running_(true) // Инициализируем флаг
{
    socket_.bind(udp::endpoint(udp::v4(), port_));
}

void UDPFileReceiver::start() {
    socket_.async_receive_from(
        buffer(receive_buffer_), 
        remote_endpoint_,
        [this](boost::system::error_code ec, std::size_t bytes_recvd) {
            if (!ec && bytes_recvd > 0 && this->is_running_) { // Используем this->
                std::ofstream file(save_dir_ + "/test_file.txt", 
                                 std::ios::binary | std::ios::app);
                file.write(receive_buffer_.data(), bytes_recvd);
                start();
            }
        });
}

void UDPFileReceiver::stop() {
    is_running_ = false;
    socket_.close();
}