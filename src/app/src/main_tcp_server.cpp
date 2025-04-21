// src/main_tcp_server.cpp - ОБНОВЛЕННАЯ ВЕРСИЯ
#include "tcp/tcp_server.hpp"
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <string>
#include <vector>
#include <condition_variable> // Для ожидания остановки
#include <mutex>             // Для ожидания остановки

// --- (Функция parse_args как раньше) ---
struct ServerConfig {
    std::string db_path = tcp_messenger::DEFAULT_DB_PATH;
    unsigned short app_port = tcp_messenger::DEFAULT_APP_PORT;
    unsigned short metrics_port = tcp_messenger::DEFAULT_METRICS_PORT;
};
ServerConfig parse_args(int argc, char* argv[]) { /* ... реализация ... */
    ServerConfig config;
    std::vector<std::string> args(argv + 1, argv + argc);
    for (size_t i = 0; i < args.size(); ++i) {
        if ((args[i] == "-dbpath" || args[i] == "--database") && i + 1 < args.size()) {
            config.db_path = args[++i];
        } else if ((args[i] == "-port" || args[i] == "--app-port") && i + 1 < args.size()) {
            try { config.app_port = std::stoi(args[++i]); } catch (...) { std::cerr << "Warning: Invalid app port value ignored: " << args[i] << std::endl; }
        } else if ((args[i] == "-mport" || args[i] == "--metrics-port") && i + 1 < args.size()) {
             try { config.metrics_port = std::stoi(args[++i]); } catch (...) { std::cerr << "Warning: Invalid metrics port value ignored: " << args[i] << std::endl; }
        } else { std::cerr << "Warning: Unknown argument ignored: " << args[i] << std::endl; }
    }
    return config;
}
// ---

int main(int argc, char* argv[]) {
    std::mutex mtx;
    std::condition_variable cv;
    bool stop_requested = false;

    try {
        ServerConfig config = parse_args(argc, argv);
        unsigned int thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) thread_count = 2;

        boost::asio::io_context io_context(thread_count);

        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code& /*error*/, int /*signal_number*/) {
            std::cout << "\n[Main] Signal received, initiating stop..." << std::endl;
            {
                std::lock_guard<std::mutex> lock(mtx);
                stop_requested = true;
            }
            cv.notify_one();
        });

        tcp_messenger::TCPServer server(io_context, config.db_path, config.app_port, config.metrics_port);
        server.start();

        std::cout << "[Main] TCP Server started. App port: " << config.app_port
                  << ", Metrics port: " << config.metrics_port
                  << ", DB path: " << config.db_path << std::endl;
        std::cout << "[Main] Using " << thread_count << " IO threads. Waiting for stop signal (SIGINT/SIGTERM)..." << std::endl;

        std::vector<std::thread> threads;
        for (unsigned int i = 0; i < thread_count; ++i) {
            threads.emplace_back([&io_context]() {
                try { io_context.run(); }
                catch (const std::exception& e) { std::cerr << "[IO Thread Error] " << e.what() << std::endl; }
            });
        }

        // --- Ожидание сигнала остановки ---
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&]{ return stop_requested; });
        }
        // --- Сигнал получен ---

        std::cout << "[Main] Stop signal processed, stopping server components..." << std::endl;
        server.stop();
        io_context.stop(); // Теперь останавливаем io_context ПОСЛЕ вызова server.stop()

        std::cout << "[Main] Waiting for IO threads to join..." << std::endl;
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
        std::cout << "[Main] IO threads joined." << std::endl;
        std::cout << "[Main] Server stopped completely." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[Main] Server critical exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[Main] Unknown server critical exception." << std::endl;
        return 1;
    }
    return 0;
}
