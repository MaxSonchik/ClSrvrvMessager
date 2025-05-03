// src/main_tcp_server.cpp - Финальная рабочая версия

#include "tcp/tcp_server.hpp" // Основной класс сервера
#include <boost/asio/signal_set.hpp> // Для обработки сигналов ОС
#include <boost/asio/executor_work_guard.hpp> // Для удержания io_context.run()
#include <iostream> // Для вывода в консоль/логи
#include <stdexcept> // Для std::exception
#include <string> // Для std::string
#include <vector> // Для std::vector (парсинг аргументов)

// Структура для хранения конфигурации сервера, полученной из аргументов или по умолчанию
struct ServerConfig {
    std::string db_path = tcp_messenger::DEFAULT_DB_PATH;
    unsigned short app_port = tcp_messenger::DEFAULT_APP_PORT;
    unsigned short metrics_port = tcp_messenger::DEFAULT_METRICS_PORT;
};

// Простая функция для парсинга аргументов командной строки
// Поддерживает: -dbpath <path>, -port <num>, -mport <num>
ServerConfig parse_args(int argc, char* argv[]) {
    ServerConfig config;
    std::vector<std::string> args(argv + 1, argv + argc); // Собираем аргументы в вектор

    for (size_t i = 0; i < args.size(); ++i) {
        // Путь к базе данных
        if ((args[i] == "-dbpath" || args[i] == "--database") && i + 1 < args.size()) {
            config.db_path = args[++i]; // Берем следующий аргумент как путь
        }
        // Порт приложения
        else if ((args[i] == "-port" || args[i] == "--app-port") && i + 1 < args.size()) {
            try {
                int port_val = std::stoi(args[++i]); // Конвертируем в int
                // Проверяем допустимый диапазон портов (1-65535)
                if (port_val > 0 && port_val <= 65535) {
                     config.app_port = static_cast<unsigned short>(port_val);
                } else {
                     // Выводим предупреждение в stderr (попадет в journalctl)
                     std::cerr << "Warning: Invalid app port value ignored: " << port_val << std::endl;
                }
            } catch (const std::exception& e) {
                 std::cerr << "Warning: Invalid app port value ignored: " << args[i] << " (" << e.what() << ")" << std::endl;
            }
        }
        // Порт метрик Prometheus
        else if ((args[i] == "-mport" || args[i] == "--metrics-port") && i + 1 < args.size()) {
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
        }
        // Неизвестный аргумент
        else {
            std::cerr << "Warning: Unknown argument ignored: " << args[i] << std::endl;
        }
    }
    return config; // Возвращаем структуру с конфигурацией
}


int main(int argc, char* argv[]) {
    std::cout << "[Main] Server process starting..." << std::endl; // Самое первое сообщение

    try {
        // 1. Парсим аргументы командной строки
        ServerConfig config = parse_args(argc, argv);
        std::cout << "[Main] Using configuration - DB: '" << config.db_path
                  << "', App Port: " << config.app_port
                  << ", Metrics Port: " << config.metrics_port << std::endl;

        // 2. Создаем основной контекст ввода-вывода Asio
        boost::asio::io_context io_context;

        // 3. Создаем "хранителя работы" (work guard)
        // Это гарантирует, что io_context.run() не завершится немедленно,
        // если в очереди нет активных обработчиков.
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard(io_context.get_executor());

        // 4. Настраиваем обработку сигналов для корректной остановки
        // Ловим SIGINT (Ctrl+C) и SIGTERM (стандартный сигнал остановки от systemd/kill)
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code& error, int signal_number) {
            if (error) {
                // Редко, но может случиться ошибка при ожидании сигнала
                 std::cerr << "[Main Signal Handler] Error waiting for signal: " << error.message() << std::endl;
                 // Все равно пытаемся остановить
            } else {
                 std::cout << "\n[Main Signal Handler] Signal " << signal_number << " received. Initiating shutdown..." << std::endl;
            }
            // Говорим io_context остановиться. Это прервет io_context.run().
            io_context.stop();
        });

        // 5. Создаем экземпляр нашего TCP сервера
        tcp_messenger::TCPServer server(io_context, config.db_path, config.app_port, config.metrics_port);

        // 6. Запускаем асинхронные операции сервера (прием соединений, таймеры)
        server.start(); // Этот вызов не блокирует, он лишь постит задачи в io_context

        std::cout << "[Main] Server initialization complete. Running event loop..." << std::endl;
        std::cout.flush(); // Гарантируем вывод перед блокировкой

        // 7. Запускаем основной цикл обработки событий Asio
        // Эта строка будет БЛОКИРОВАТЬ выполнение основного потока до тех пор,
        // пока io_context.stop() не будет вызван (обычно из обработчика сигнала).
        // Наличие work_guard предотвращает немедленный выход.
        io_context.run();

        // --- Выполнение продолжается здесь только ПОСЛЕ остановки io_context ---

        std::cout << "[Main] IO context stopped. Cleaning up server resources..." << std::endl;
        // 8. Вызываем метод stop нашего сервера для корректного закрытия
        //    соединений, отмены таймеров и т.д.
        server.stop();
        std::cout << "[Main] Server cleanup finished. Exiting." << std::endl;

    } catch (const std::exception& e) {
        // Ловим любые исключения, возникшие при инициализации или работе
        std::cerr << "[Main CRITICAL ERROR] Exception caught: " << e.what() << std::endl;
        return 1; // Возвращаем код ошибки
    } catch (...) {
        // Ловим любые другие (не стандартные) исключения
        std::cerr << "[Main CRITICAL ERROR] Unknown exception caught." << std::endl;
        return 1; // Возвращаем код ошибки
    }

    std::cout << "[Main] Process finished normally." << std::endl;
    return 0; // Успешное завершение
}
