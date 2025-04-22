// src/main_tcp_server.cpp - ОТЛАДОЧНАЯ ВЕРСИЯ С ЛОГАМИ
#include "tcp/tcp_server.hpp"
#include <boost/asio/signal_set.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

struct ServerConfig {
    std::string db_path = tcp_messenger::DEFAULT_DB_PATH;
    unsigned short app_port = tcp_messenger::DEFAULT_APP_PORT;
    unsigned short metrics_port = tcp_messenger::DEFAULT_METRICS_PORT;
};

ServerConfig parse_args(int argc, char* argv[]) {
    ServerConfig config;
    std::vector<std::string> args(argv + 1, argv + argc);
    for (size_t i = 0; i < args.size(); ++i) {
        if ((args[i] == "-dbpath" || args[i] == "--database") && i + 1 < args.size()) { config.db_path = args[++i]; }
        else if ((args[i] == "-port" || args[i] == "--app-port") && i + 1 < args.size()) { try { int p = std::stoi(args[++i]); if (p > 0 && p <= 65535) config.app_port = p; } catch (...) {} }
        else if ((args[i] == "-mport" || args[i] == "--metrics-port") && i + 1 < args.size()) { try { int p = std::stoi(args[++i]); if (p > 0 && p <= 65535) config.metrics_port = p; } catch (...) {} }
    }
    return config;
}

int main(int argc, char* argv[]) {
    // Добавляем вывод сразу при старте main
    std::cout << "[Main DEBUG] Process started." << std::endl; std::cout.flush(); // Сбрасываем буфер

    try {
        ServerConfig config = parse_args(argc, argv);
        std::cout << "[Main DEBUG] Arguments parsed." << std::endl; std::cout.flush();

        boost::asio::io_context io_context;
        std::cout << "[Main DEBUG] io_context created." << std::endl; std::cout.flush();

        // Создаем Work Guard
        auto work_guard = boost::asio::make_executor_work_guard(io_context.get_executor());
        std::cout << "[Main DEBUG] Work guard created." << std::endl; std::cout.flush();

        // Настраиваем сигналы
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code& error, int signal_number) {
             std::cout << "\n[Main DEBUG Signal Handler] Signal " << signal_number << " received.";
             if(error) std::cout << " Error: " << error.message();
             std::cout << " Stopping io_context..." << std::endl; std::cout.flush();
             // work_guard.reset(); // Можно раскомментировать, если stop() не сработает
             io_context.stop();
        });
        std::cout << "[Main DEBUG] Signal handler set up." << std::endl; std::cout.flush();

        // Создаем сервер
        tcp_messenger::TCPServer server(io_context, config.db_path, config.app_port, config.metrics_port);
        std::cout << "[Main DEBUG] TCPServer instance created." << std::endl; std::cout.flush();

        // Стартуем сервер (постим async операции)
        server.start();
        std::cout << "[Main DEBUG] server.start() completed." << std::endl; std::cout.flush();


        std::cout << "[Main] TCP Server started. App port: " << config.app_port
                  << ", Metrics port: " << config.metrics_port
                  << ", DB path: " << config.db_path << std::endl;
        std::cout << "[Main] Running event loop. Press Ctrl+C to exit." << std::endl; std::cout.flush();

        // Проверяем состояние io_context перед запуском
        if (io_context.stopped()) {
             std::cerr << "[Main DEBUG CRITICAL] io_context is stopped BEFORE run()!" << std::endl; std::cerr.flush();
        } else {
             std::cout << "[Main DEBUG] io_context is NOT stopped. Calling run()..." << std::endl; std::cout.flush();
        }

        // Запускаем цикл обработки событий
        io_context.run(); // Должен блокировать здесь!

        // Этот код выполнится ТОЛЬКО ПОСЛЕ возврата из io_context.run()
        std::cout << "[Main DEBUG] io_context.run() returned." << std::endl; std::cout.flush();

        std::cout << "[Main] IO context stopped. Cleaning up server resources..." << std::endl; std::cout.flush();
        server.stop();
        std::cout << "[Main] Server cleanup finished. Exiting." << std::endl; std::cout.flush();

    } catch (const std::exception& e) {
        std::cerr << "[Main] Server critical exception: " << e.what() << std::endl; std::cerr.flush();
        return 1;
    } catch (...) {
        std::cerr << "[Main] Unknown server critical exception." << std::endl; std::cerr.flush();
        return 1;
    }
    std::cout << "[Main DEBUG] Process exiting normally (return 0)." << std::endl; std::cout.flush();
    return 0;
}
