#include "../include/common.hpp"
#include "../include/tcp_client.hpp"
#include <iostream>
#include <string>

int main() {
    try {
        std::string server_ip = "95.24.129.89";
        std::string username;
        std::string password;

        // Запрос логина и пароля у пользователя
        std::cout << "Enter username: ";
        std::cin >> username;
        std::cout << "Enter password: ";
        std::cin >> password;

        // Подключение к серверу
        TCPClient client(server_ip, SERVER_PORT);
        client.connect();  // Сообщение об успешном подключении
        std::cout << "Connected to server successfully!" << std::endl;

        // Формирование и отправка сообщения о регистрации
        Message reg_msg;
        reg_msg.type = MessageType::ClientRegistration;
        reg_msg.sender = username;
        reg_msg.password = password;

        client.send_message(reg_msg);

        // Получаем ответ от сервера
        Message reply = client.receive_message();

        if (reply.text == "Ok") {
            std::cout << "Authentication successful! Welcome, " << username << "!" << std::endl;
            // Здесь начинается основная логика чата
        } else if (reply.text == "Error") {
            std::cerr << "Authentication failed: incorrect password or user." << std::endl;
            return 1;
        }

    } catch (std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
