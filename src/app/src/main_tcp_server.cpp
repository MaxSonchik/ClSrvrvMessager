// src/main_tcp_server.cpp
#include "tcp/tcp_server.hpp"
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <string>
#include <vector>
#include <condition_variable>
#include <mutex>

// --- (Функция parse_args как раньше) ---
struct ServerConfig { /* ... */ };
ServerConfig parse_args(int argc, char* argv[]) { /* ... */ }
// ---

int main(int argc, char* argv[]) {
    std::mutex mtx;
    std::condition_variable cv;
    bool stop_requested = false;

    try {
        // ... config parsing, io_context setup ...

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
            threads.emplace_back([&io_context]() { /* ... */ });
        }

        // --- Ожидание сигнала остановки ---
        std::cout << "[Main] Entering wait loop..." << std::endl; // <-- ЛОГ ПЕРЕД ОЖИДАНИЕМ
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&]{ return stop_requested; });
        }
        std::cout << "[Main] Exited wait loop." << std::endl; // <-- ЛОГ ПОСЛЕ ОЖИДАНИЯ
        // --- Сигнал получен ---

        std::cout << "[Main] Stop signal processed, stopping server components..." << std::endl;
        server.stop();
        io_context.stop();

        std::cout << "[Main] Waiting for IO threads to join..." << std::endl;
        for (auto& t : threads) { /* ... */ }
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
