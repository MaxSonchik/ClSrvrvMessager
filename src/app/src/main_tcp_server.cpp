// src/main_tcp_server.cpp - ИСПРАВЛЕННАЯ ВЕРСИЯ
#include "tcp/tcp_server.hpp"
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
// Убираем #include <thread> - больше не нужен

// ... (struct ServerConfig и parse_args как были) ...

int main(int argc, char* argv[]) {
    try {
        ServerConfig config = parse_args(argc, argv);

        // Используем один io_context (без указания кол-ва потоков)
        boost::asio::io_context io_context;

        // Обработчик сигналов для корректной остановки
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code& /*error*/, int signal_number) {
            std::cout << "\n[Main] Signal " << signal_number << " received, stopping io_context..." << std::endl;
            io_context.stop(); // Говорим io_context::run() завершиться
        });

        tcp_messenger::TCPServer server(io_context, config.db_path, config.app_port, config.metrics_port);

        server.start(); // Запускает acceptor, таймеры и т.д.

        std::cout << "[Main] TCP Server started. App port: " << config.app_port
                  << ", Metrics port: " << config.metrics_port
                  << ", DB path: " << config.db_path << std::endl;
        std::cout << "[Main] Running event loop. Press Ctrl+C to exit." << std::endl;

        // !!! ЗАПУСКАЕМ io_context.run() В ОСНОВНОМ ПОТОКЕ !!!
        // Эта строка будет блокировать выполнение до вызова io_context.stop()
        io_context.run();

        // Код ниже выполнится ТОЛЬКО ПОСЛЕ того, как io_context.stop()
        // будет вызван (обычно обработчиком сигнала)
        std::cout << "[Main] IO context stopped. Cleaning up..." << std::endl;
        server.stop(); // Вызываем stop сервера для очистки (закрытия сокетов и т.д.)
        std::cout << "[Main] Server cleanup finished." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[Main] Server critical exception: " << e.what() << std::endl;
        // Запись в stderr должна попасть в journalctl
        return 1;
    } catch (...) {
        std::cerr << "[Main] Unknown server critical exception." << std::endl;
        return 1;
    }
    return 0;
}
