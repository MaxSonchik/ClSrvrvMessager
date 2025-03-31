
#include "tcp_client.hpp"
#include "common/json_util.h"
#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <string>
#include <nlohmann/json.hpp>

namespace tcp_messenger {

using boost::asio::ip::tcp;
using json = nlohmann::json;

TCPClient::TCPClient(const std::string& username, const std::string& server_host, unsigned short server_port)
    : username_(username), server_host_(server_host), server_port_(server_port),
      tcp_socket_(io_context_), udp_socket_(io_context_), running_(false) {
}

TCPClient::~TCPClient() {
    disconnect();
}

void TCPClient::run() {
    try {
        tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(server_host_, std::to_string(server_port_));
        boost::asio::connect(tcp_socket_, endpoints);

        running_ = true;

        // Отправка события подключения на сервер
        std::string connect_msg = common::make_json_event("connect", username_) + "\n";
        boost::asio::write(tcp_socket_, boost::asio::buffer(connect_msg));

        // Поток чтения с сервера
        std::thread reader([this]() {
            while (running_) {
                boost::asio::streambuf buffer;
                boost::system::error_code ec;
                boost::asio::read_until(tcp_socket_, buffer, "\n", ec);
                if (ec) {
                    std::cerr << "[TCPClient] Connection lost: " << ec.message() << std::endl;
                    running_ = false;
                    break;
                }

                std::istream is(&buffer);
                std::string line;
                std::getline(is, line);
                if (!line.empty()) {
                    try {
                        json data = json::parse(line);
                        std::string event = data.value("event", "");
                        if (event == "message") {
                            std::string from = data.value("from", "");
                            std::string text = data.value("text", "");
                            std::cout << "\n[from: " << from << "] " << text << std::endl;
                        } else if (event == "error") {
                            std::cerr << "[error] " << data.value("text", "") << std::endl;
                        } else {
                            std::cout << "[server] " << line << std::endl;
                        }
                    } catch (...) {
                        std::cerr << "[client] Invalid JSON: " << line << std::endl;
                    }
                }
            }
        });

        // Основной цикл отправки сообщений
        std::cin.ignore(); // очищаем ввод после ввода username
        while (running_) {
            std::string to, text;
            std::cout << "To: ";
            std::getline(std::cin, to);
            std::cout << "Message: ";
            std::getline(std::cin, text);

            if (to == "/exit" || text == "/exit") {
                break;
            }

            std::string msg = common::make_json_event("message", username_, to, text) + "\n";
            boost::asio::write(tcp_socket_, boost::asio::buffer(msg));
        }

        running_ = false;
        tcp_socket_.close();
        if (reader.joinable()) reader.join();

    } catch (std::exception& e) {
        std::cerr << "Client exception: " << e.what() << std::endl;
    }
}

void TCPClient::disconnect() {
    if (running_) {
        running_ = false;
        boost::system::error_code ec;
        tcp_socket_.close(ec);
    }
}

} // namespace tcp_messenger
