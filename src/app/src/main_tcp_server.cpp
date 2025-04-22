// src/main_tcp_server.cpp - ИСПРАВЛЕННАЯ ВЕРСИЯ
#include "tcp/tcp_server.hpp"
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Структура для хранения конфигурации сервера
struct ServerConfig {
    std::string db_path = tcp_messenger::DEFAULT_DB_PATH;
    unsigned short app_port = tcp_messenger::DEFAULT_APP_PORT;
    unsigned short metrics_port = tcp_messenger::DEFAULT_METRICS_PORT;
};

// Функция для парсинга аргументов командной строки
ServerConfig parse_args(int argc, char* argv[]) {
    ServerConfig config;
    std::vector<std::string> args(argv + 1, argv + argc);

    for (size_t i = 0; i < args.size(); ++i) {
        if ((args[i] == "-dbpath" || args[i] == "--database") && i + 1 < args.size()) {
            config.db_path = args[++i];
        } else if ((args[i] == "-port" || args[i] == "--app-port") && i + 1 < args.size()) {
            try {
                int port_val = std::stoi(args[++i]);
                // Проверка диапазона порта
                if (port_val > 0 && port_val <= 65535) {
                     config.app_port = static_cast<unsigned short>(port_val);
                } else {
                     std::cerr << "Warning: Invalid app port value ignored: " << port_val << std::endl;
                }
            } catch (const std::exception& e) {
                 std::cerr << "Warning: Invalid app port value ignored: " << args[i] << " (" << e.what() << ")" << std::endl;
            }
        } else if ((args[i] == "-mport" || args[i] == "--metrics-port") && i + 1 < args.size()) {
             try {
                 int port_val = std::stoi(args[++i]);
                  if (port_val > 0 && port_val <= 65535) {
                     config.metrics_port = static_cast<unsigned short>(port_val);
                 } else {
                     std::cerr << "Warning: Invalid metrics port value ignored: " << port_val << std::endl;
                 }
            } catch (const std::exception& e) {
                 std::cerr << "Warning: Invalid metrics port value ignored: " << args[i] << " (" << e.what() << ")" << std::endl;
            }
        } else {
            std::cerr << "Warning: Unknown argument ignored: " << args[i] << std::endl;
        }
    }
    return config;
}


int main(int argc, char* argv[]) {
    try {
        ServerConfig config = parse_args(argc, argv);

        // Используем один io_context
        boost::asio::io_context io_context;

        // Обработчик сигналов SIGINT (Ctrl+C) и SIGTERM (стандартный сигнал остановки)
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code& /*error*/, int signal_number) {
            // Этот код выполнится при получении сигнала
            std::cout << "\n[Main] Signal " << signal_number << " received, stopping io_context..." << std::endl;
            // Останавливаем io_context. Это приведет к возврату из io_context.run() ниже.
            io_context.stop();
        });

        // Создаем и инициализируем сервер
        tcp_messenger::TCPServer server(io_context, config.db_path, config.app_port, config.metrics_port);

        // Запускаем асинхронные операции сервера (accept, таймеры)
        server.start();

        std::cout << "[Main] TCP Server started. App port: " << config.app_port
                  << ", Metrics port: " << config.metrics_port
                  << ", DB path: " << config.db_path << std::endl;
        std::cout << "[Main] Running event loop. Press Ctrl+C to exit." << std::endl;

        // Запускаем основной цикл обработки событий Asio в этом потоке.
        // Он будет работать, пока есть активные асинхронные операции
        // или пока не будет вызван io_context.stop() (через обработчик сигнала).
        io_context.run();

        // --- Код ниже выполнится только после остановки io_context ---

        std::cout << "[Main] IO context stopped. Cleaning up server resources..." << std::endl;
        // Вызываем stop сервера для корректного закрытия сокетов, отмены таймеров и т.д.
        server.stop();
        std::cout << "[Main] Server cleanup finished. Exiting." << std::endl;

    } catch (const std::exception& e) {
        // Логируем критические ошибки в stderr, чтобы они попали в journalctl
        std::cerr << "[Main] Server critical exception: " << e.what() << std::endl;
        return 1; // Завершаемся с кодом ошибки
    } catch (...) {
        std::cerr << "[Main] Unknown server critical exception." << std::endl;
        return 1; // Завершаемся с кодом ошибки
    }
    return 0; // Успешное завершение после штатной остановки
}
