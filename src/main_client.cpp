#include "../include/common.hpp"
#include "../include/tcp_client.hpp"
#include "../include/udp_file_sender.hpp"
#include <iostream>
#include <string>

int main() {
    try {
        // Инициализация
        std::string server_ip = "95.24.129.89";
        uint16_t server_port = SERVER_PORT;
        uint16_t udp_port = SERVER_PORT + 1;

        // Подключение клиента
        TCPClient client(server_ip, server_port);
        client.connect();

        // Авторизация пользователя
        std::string username, password;
        std::cout << "Enter username: ";
        std::cin >> username;
        std::cout << "Enter password: ";
        std::cin >> password;

        client.register_client(username, password);
        std::cout << "Connected to the server and registered!\n";

        // Основной цикл
        boost::asio::io_context ioc;
        UDPFileSender udp_sender(ioc);

        while (true) {
            std::string input;
            std::cout << "> ";
            std::getline(std::cin >> std::ws, input);

            if (input == "/exit") {
                std::cout << "Exiting...\n";
                break;
            } else if (input.compare(0, 11, "/send_file ") == 0) {
                std::string file_path = input.substr(11);
                if (udp_sender.send_file(file_path, server_ip, udp_port)) {
                    std::cout << "File sent successfully!\n";
                } else {
                    std::cerr << "Failed to send file.\n";
                }
            } else {
                // Отправка обычного сообщения
                Message msg;
                msg.type = MessageType::Text;
                msg.sender = username;
                msg.text = input;

                client.send_message(msg);
                Message reply = client.receive_message();
                std::cout << "Server: " << reply.text << "\n";
            }
        }

    } catch (std::exception &e) {
        log_error("Error: " + std::string(e.what()));
    }

    return 0;
}
