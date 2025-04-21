// src/main_tcp_server.cpp - ОБНОВЛЕННАЯ ВЕРСИЯ
#include "tcp/tcp_server.hpp"
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <string>
#include <vector>

// Функция для простого парсинга аргументов
struct ServerConfig {
    std::string db_path = tcp_messenger::DEFAULT_DB_PATH;
    unsigned short app_port = tcp_messenger::DEFAULT_APP_PORT;
    unsigned short metrics_port = tcp_messenger::DEFAULT_METRICS_PORT;
};
//ExecStart=/opt/messenger/messenger_tcp_server -dbpath data/messenger.db -port 8080 -mport 9090
ServerConfig parse_args(int argc, char* argv[]) {
    ServerConfig config;
    std::vector<std::string> args(argv + 1, argv + argc); // Копируем аргументы

    for (size_t i = 0; i < args.size(); ++i) {
        if ((args[i] == "-dbpath" || args[i] == "--database") && i + 1 < args.size()) {
            config.db_path = args[++i];
        } else if ((args[i] == "-port" || args[i] == "--app-port") && i + 1 < args.size()) {
            try {
                config.app_port = std::stoi(args[++i]);
            } catch (...) { std::cerr << "Warning: Invalid app port value ignored: " << args[i] << std::endl; }
        } else if ((args[i] == "-mport" || args[i] == "--metrics-port") && i + 1 < args.size()) {
             try {
                config.metrics_port = std::stoi(args[++i]);
            } catch (...) { std::cerr << "Warning: Invalid metrics port value ignored: " << args[i] << std::endl; }
        } else {
            std::cerr << "Warning: Unknown argument ignored: " << args[i] << std::endl;
        }
    }
    return config;
}


int main(int argc, char* argv[]) { // <-- Теперь принимаем argc, argv
    try {
        // Парсим аргументы командной строки
        ServerConfig config = parse_args(argc, argv);

        unsigned int thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) thread_count = 2;

        boost::asio::io_context io_context(thread_count);
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int){
            io_context.stop();
        });

        // Используем конфигурацию из аргументов ИЛИ значения по умолчанию
        tcp_messenger::TCPServer server(io_context, config.db_path, config.app_port, config.metrics_port);

        server.start();

        // Выводим фактические используемые параметры
        std::cout << "[Main] TCP Server started. App port: " << config.app_port
                  << ", Metrics port: " << config.metrics_port
                  << ", DB path: " << config.db_path << std::endl; // Используем config
        std::cout << "[Main] Using " << thread_count << " IO threads. Press Ctrl+C to exit." << std::endl;


        std::vector<std::thread> threads;
        for (unsigned int i = 0; i < thread_count; ++i) {
            threads.emplace_back([&io_context]() { /* ... */ });
        }
        for (auto& t : threads) { if (t.joinable()) t.join(); }

        server.stop(); // Попытка очистки
        std::cout << "[Main] Server stopped." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[Main] Server exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[Main] Unknown server exception." << std::endl;
        return 1;
    }
    return 0;
}
