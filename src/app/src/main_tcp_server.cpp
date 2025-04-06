// main_tcp_server.cpp
#include "tcp/tcp_server.hpp" // <-- Убедитесь, что путь правильный
#include <boost/asio.hpp>
#include <iostream>
#include <stdexcept> // Для std::exception

int main() {
    try {
        boost::asio::io_context io_context;

        // Вызываем конструктор БЕЗ указания портов и пути к БД,
        // используются значения по умолчанию из tcp_server.hpp
        tcp_messenger::TCPServer server(io_context);

        server.start(); // Запуск приема соединений

        // io_context.run() блокирует поток и выполняет асинхронные операции
        // Сервер будет работать, пока io_context не будет остановлен
        std::cout << "TCP Server started. App port: " << tcp_messenger::DEFAULT_APP_PORT
                  << ", Metrics port: " << tcp_messenger::DEFAULT_METRICS_PORT
                  << ", DB path: " << tcp_messenger::DEFAULT_DB_PATH << std::endl;
        std::cout << "Press Ctrl+C to exit." << std::endl;

        io_context.run(); // Запускаем обработку событий

    } catch (const std::exception& e) { // Ловим const ссылку
        std::cerr << "Server exception: " << e.what() << std::endl;
        return 1; // Возвращаем код ошибки
    }
    return 0;
}