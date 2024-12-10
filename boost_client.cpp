#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <string>

using boost::asio::ip::tcp;

class Client {
public:
    Client(boost::asio::io_context& io_context, const std::string& host, short tcp_port)
        : socket_(io_context) {
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(host, std::to_string(tcp_port));
        boost::asio::connect(socket_, endpoints);
    }

    void run(int client_id) {
        // Отправляем ID клиента серверу
        boost::asio::write(socket_, boost::asio::buffer(std::to_string(client_id)));

        std::thread reader_thread([this]() { read_messages(); });

        chat_mode();

        reader_thread.join();
    }

private:
    void read_messages() {
        try {
            char data[1024];
            while (true) {
                size_t length = socket_.read_some(boost::asio::buffer(data));
                std::cout << "Message received: " << std::string(data, length) << std::endl;
            }
        } catch (std::exception& e) {
            std::cerr << "Disconnected: " << e.what() << std::endl;
        }
    }

    void chat_mode() {
        try {
            std::cout << "Enter your messages. Type 'EXIT' to quit.\n";
            while (true) {
                std::string message;
                std::getline(std::cin, message);

                if (message == "EXIT") {
                    boost::asio::write(socket_, boost::asio::buffer("EXIT"));
                    break;
                }

                boost::asio::write(socket_, boost::asio::buffer(message));
            }
        } catch (std::exception& e) {
            std::cerr << "Error while sending message: " << e.what() << std::endl;
        }
    }

    tcp::socket socket_;
};

int main() {
    try {
        const std::string host = "95.24.130.86"; // Замените на нужный IP
        const short port = 12345; // Замените на нужный порт

        boost::asio::io_context io_context;
        Client client(io_context, host, port);
        static int client_id = 1; // Генерация ID клиента
        client.run(client_id++);
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
