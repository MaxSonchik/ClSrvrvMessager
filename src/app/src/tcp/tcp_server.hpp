#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include "../database/database_manager.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <iostream>
#include <chrono> // Для времени
#include <ctime>  // Для времени
#include <iomanip> // Для форматирования времени
#include <sstream> // Для форматирования времени

#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include <prometheus/gauge.h>

namespace tcp_messenger {

// Объявляем using для tcp после включения asio
using boost::asio::ip::tcp;

// Константы для портов и пути к БД по умолчанию
const unsigned short DEFAULT_APP_PORT = 8080;
const unsigned short DEFAULT_METRICS_PORT = 9090;
const std::string DEFAULT_DB_PATH = "/data/messenger.db";

// Вспомогательная функция для получения текущего времени в логах
inline std::string get_log_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

// Основной класс TCP сервера
class TCPServer {
public:
    // Вложенный класс для управления сессией клиента (определение в .cpp)
    class Session;

    // Конструктор
    TCPServer(boost::asio::io_context& io_context,
              const std::string& db_path = DEFAULT_DB_PATH,
              unsigned short app_port = DEFAULT_APP_PORT,
              unsigned short metrics_port = DEFAULT_METRICS_PORT);

    // Запуск сервера
    void start();

    // Остановка сервера
    void stop();

private:
    friend class Session; // Разрешаем доступ к приватным членам

    // Прием нового соединения
    void do_accept();

    // Обработка сообщения от клиента
    void handle_message(const std::string& message, std::shared_ptr<Session> session);

    // Удаление клиента
    void remove_client(const std::string& username, std::shared_ptr<Session> session); // Добавим session для сравнения

    // Члены класса TCPServer
    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    DatabaseManager db_manager_;
    std::unordered_map<std::string, std::shared_ptr<Session>> clients_; // Карта <username, session_ptr>
    std::mutex clients_mutex_; // Мьютекс для карты клиентов

    // Prometheus
    prometheus::Exposer exposer_;
    std::shared_ptr<prometheus::Registry> registry_;
    prometheus::Counter* messages_total_counter_;
    prometheus::Gauge* active_connections_gauge_;
};

} // namespace tcp_messenger

#endif // TCP_SERVER_HPP