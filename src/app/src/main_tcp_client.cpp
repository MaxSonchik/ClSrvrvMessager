// main_tcp_client.cpp
#include "tcp/tcp_client.hpp" // <-- Убедитесь, что путь правильный
#include <iostream>
#include <string>
#include <limits> // Для numeric_limits
#include <stdexcept> // Для std::exception

int main() {
    std::string username, host;
    unsigned short port;

    try {
        std::cout << "Enter username: ";
        std::getline(std::cin, username); // Используем getline для имен с пробелами

        std::cout << "Enter server IP (e.g., 127.0.0.1): ";
        std::getline(std::cin, host);

        std::cout << "Enter server port (e.g., 8080): ";
        while (!(std::cin >> port)) { // Проверка корректности ввода порта
            std::cout << "Invalid port. Please enter a number: ";
            std::cin.clear(); // Сброс флага ошибки
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Очистка буфера ввода
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Очистка буфера после ввода порта

        // Создаем и запускаем клиент
        tcp_messenger::TCPClient client(username, host, port);
        client.run(); // Метод run теперь содержит цикл чтения/записи

    } catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "An unknown error occurred." << std::endl;
        return 1;
    }

    std::cout << "Client finished." << std::endl;
    return 0;
}