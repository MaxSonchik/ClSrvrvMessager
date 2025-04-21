#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include "../database/database_manager.hpp" // Используем обновленный DB manager
#include "../common/json_util.h" // Используем обновленный json util
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp> // Для планировщика
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <map> // Для хранения таймеров по task_id
#include <utility>
#include <deque>
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <optional> // Для std::optional

#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include <prometheus/gauge.h>

#include <nlohmann/json.hpp> // Используем nlohmann/json

namespace tcp_messenger {

using boost::asio::ip::tcp;
using json = nlohmann::json;


const unsigned short DEFAULT_APP_PORT = 8080;
const unsigned short DEFAULT_METRICS_PORT = 9090;
const std::string DEFAULT_DB_PATH = "messenger.db";
const int DEFAULT_CLEANUP_DAYS = 30; // Удалять неактивные задачи старше 30 дней
const int DEFAULT_CLEANUP_INTERVAL_HOURS = 24; // Запускать очистку раз в 24 часа

inline std::string get_log_timestamp() {

    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

class TCPServer {
public:

    class Session : public std::enable_shared_from_this<Session> {
    public:
        Session(TCPServer& server, tcp::socket socket);
        ~Session();

        void start();
        void send(const json& data); // Теперь отправляем json объект
        void close_socket();

        std::string get_username() const;
        std::pair<std::string, unsigned short> get_remote_address() const;
        bool is_authenticated() const;
        std::string get_session_id(bool prefer_username = false) const;

        // Сессия должна знать свое имя пользователя после аутентификации
        void set_authenticated_user(const std::string& name);

    private:
        void do_read();
        void do_write(); // Добавим очередь записи для надежности

        TCPServer& server_;
        tcp::socket socket_;
        boost::asio::streambuf buffer_;
        std::deque<std::string> write_msgs_;
        std::string username_;
        bool authenticated_ = false;
        std::string remote_ep_str_;
    };


    TCPServer(boost::asio::io_context& io_context,
              const std::string& db_path = DEFAULT_DB_PATH,
              unsigned short app_port = DEFAULT_APP_PORT,
              unsigned short metrics_port = DEFAULT_METRICS_PORT);

    void start();
    void stop();

    void schedule_task_notification(const Task& task);
    void cancel_task_notification(int task_id);

private:
    friend class Session;

    void do_accept();
    void handle_message(const std::string& raw_message, std::shared_ptr<Session> session);


    void handle_register(const json& data, std::shared_ptr<Session> session);
    void handle_login(const json& data, std::shared_ptr<Session> session);
    void handle_message_event(const json& data, std::shared_ptr<Session> session);
    void handle_add_task(const json& data, std::shared_ptr<Session> session);
    void handle_list_tasks(const json& data, std::shared_ptr<Session> session);
    // TODO: Добавить обработчики для передачи файлов: file_offer, file_accept, file_data и т.д.
    // void handle_file_offer(const json& data, std::shared_ptr<Session> session);
    // ...

    void remove_client(const std::string& username, std::shared_ptr<Session> session);

    void load_and_schedule_tasks();
    void start_cleanup_timer();
    void perform_db_cleanup(const boost::system::error_code& ec);


    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    DatabaseManager db_manager_;
    std::unordered_map<std::string, std::shared_ptr<Session>> clients_;
    std::mutex clients_mutex_;

    prometheus::Exposer exposer_;
    std::shared_ptr<prometheus::Registry> registry_;
    prometheus::Counter* messages_total_counter_;
    prometheus::Gauge* active_connections_gauge_;


    std::map<int, std::shared_ptr<boost::asio::steady_timer>> task_timers_;
    std::mutex task_timers_mutex_;
    boost::asio::steady_timer cleanup_timer_;
};

} // namespace tcp_messenger

#endif // TCP_SERVER_HPP
