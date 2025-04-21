#include "tcp/tcp_server.hpp"
#include <boost/asio/signal_set.hpp> // Для обработки сигналов Ctrl+C
#include <iostream>
#include <stdexcept>
#include <thread> // Для std::thread::hardware_concurrency

int main() {
    try {
        // Определяем количество потоков для io_context
        // Используем количество ядер процессора для лучшей производительности
        // Можно ограничить, если нужно (например, 2 или 4)
        unsigned int thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) thread_count = 2; // Минимум 2 потока
        std::cout << "[Main] Using " << thread_count << " threads for IO context." << std::endl;


        boost::asio::io_context io_context(thread_count);

        // Создаем signal_set для обработки SIGINT и SIGTERM
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code& /*error*/, int /*signal_number*/) {
            std::cout << "\n[Main] Signal received, stopping server..." << std::endl;
            // Вызываем stop() сервера и затем останавливаем io_context
            // Важно: stop() сервера должен вызваться до io_context.stop()
            // чтобы успеть отменить таймеры и закрыть сокеты через post
            // Вызов server.stop() из обработчика сигнала может быть не потокобезопасен,
            // лучше использовать post для вызова stop в потоке io_context.
            // Но для простоты пока так, предполагая, что TCPServer::stop() потокобезопасен
            // или что io_context.run() еще не завершился.
            // БЕЗОПАСНЕЕ:
            // boost::asio::post(io_context, [&server](){ server.stop(); });
            // io_context.stop();
            // ПОКА ОСТАВИМ ПРЯМОЙ ВЫЗОВ, т.к. stop() использует post/mutex внутри
            // НО! Если io_context уже остановлен, post не сработает.
             // Простой вариант - просто остановить io_context, деструктор сервера должен все почистить.
             io_context.stop();
        });


        // Используем значения по умолчанию для портов и БД
        tcp_messenger::TCPServer server(io_context);

        server.start(); // Запуск приема соединений и планировщика

        std::cout << "[Main] TCP Server started. App port: " << tcp_messenger::DEFAULT_APP_PORT
                  << ", Metrics port: " << tcp_messenger::DEFAULT_METRICS_PORT
                  << ", DB path: " << tcp_messenger::DEFAULT_DB_PATH << std::endl;
        std::cout << "[Main] Press Ctrl+C to exit." << std::endl;

        // Создаем и запускаем потоки для io_context.run()
        std::vector<std::thread> threads;
        for (unsigned int i = 0; i < thread_count; ++i) {
            threads.emplace_back([&io_context]() {
                try {
                    io_context.run();
                } catch (const std::exception& e) {
                     std::cerr << "[IO Thread Error] " << e.what() << std::endl;
                }
            });
        }

        // Ждем завершения всех потоков io_context
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }

         // Вызовем stop() здесь после завершения io_context.run() для финальной очистки
         // (хотя деструктор TCPServer тоже должен это сделать)
         server.stop(); // Попытка очистки после остановки потоков

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