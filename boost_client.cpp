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

        main_menu();

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

    void main_menu() {
        try {
            while (true) {
                std::cout << "\nMain Menu:\n";
                std::cout << "1. Start chat with a user (Enter their ID)\n";
                std::cout << "2. Exit chat\n";
                std::cout << "Choice: ";
                int choice;
                std::cin >> choice;

                if (choice == 1) {
                    std::cout << "Enter recipient ID: ";
                    int recipient_id;
                    std::cin >> recipient_id;

                    // Уведомляем сервер о начале диалога
                    std::string start_command = "START:" + std::to_string(recipient_id);
                    boost::asio::write(socket_, boost::asio::buffer(start_command));

                    chat_mode();
                } else if (choice == 2) {
                    std::cout << "Exiting...\n";
                    break;
                }
            }
        } catch (std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void chat_mode() {
        try {
            std::cout << "Enter 'EXIT' to leave chat.\n";
            std::cin.ignore(); // Очистить ввод

            while (true) {
                std::string message;
                std::cout << "You: ";
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

int main(int argc, char* argv[]) {
    try {
        if (argc != 4) {
            std::cerr << "Usage: client <host> <port> <client_id>" << std::endl;
            return 1;
        }

        boost::asio::io_context io_context;
        Client client(io_context, argv[1], std::atoi(argv[2]));
        client.run(std::atoi(argv[3]));
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
