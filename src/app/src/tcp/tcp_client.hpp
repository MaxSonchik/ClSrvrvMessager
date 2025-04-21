#ifndef TCP_CLIENT_HPP // Расширение .hpp более стандартно
#define TCP_CLIENT_HPP

#include <atomic>
#include <boost/asio.hpp>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <deque> // Для очереди отправки
#include <nlohmann/json.hpp> // Для работы с JSON

namespace tcp_messenger {

using boost::asio::ip::tcp;
using json = nlohmann::json;

class TCPClient {
public:
  TCPClient(const std::string &server_host, unsigned short server_port);
  ~TCPClient();

  // Запуск клиента (потоки и т.д.)
  void start();
  // Остановка клиента
  void stop();

  // --- Асинхронные операции ---
  // Попытка регистрации
  void register_user(const std::string& username, const std::string& password);
  // Попытка входа
  void login(const std::string& username, const std::string& password);
  // Отправка сообщения
  void send_message(const std::string &to, const std::string &text);
  // Добавление задачи
  void add_task(const std::string& task_name, const std::string& description, const std::string& trigger_time, int notify_offset);
  // Запрос списка задач
  void request_task_list();
  // Отправка файла (пока только offer)
  void send_file_offer(const std::string& to, const std::string& file_path);

  // --- Обратные вызовы (Callbacks) для GUI ---
  // Устанавливаются извне (из GUI кода)
  std::function<void(bool success, const std::string& message)> on_register_result;
  std::function<void(bool success, const std::string& message)> on_login_result;
  std::function<void(const std::string& from, const std::string& text)> on_message_received;
  std::function<void(const json& task_list)> on_task_list_received; // Отправляем весь JSON
  std::function<void(const json& task_notification)> on_task_notification; // Отправляем весь JSON
  std::function<void(const std::string& error_message)> on_server_error;
  std::function<void(const std::string& status_message)> on_server_status;
  std::function<void(bool connected)> on_connection_status_changed;
  // TODO: Добавить коллбэки для событий передачи файлов

private:
  void run_io_context();
  void do_connect();
  void start_read();
  void handle_server_message(const boost::system::error_code& ec, std::size_t bytes_transferred);
  void do_write();
  void send_json(const json& data); // Метод для отправки JSON
  void close_connection(const std::string& reason);

  std::string server_host_;
  unsigned short server_port_;
  boost::asio::io_context io_context_;
  tcp::socket socket_;
  boost::asio::streambuf read_buf_;
  std::deque<std::string> write_msgs_; // Очередь JSON строк на отправку
  std::thread io_thread_;
  std::atomic<bool> running_;
  std::atomic<bool> connected_;
  std::string current_username_; // Имя пользователя после успешного логина

  // Защита для очереди записи (если send_json вызывается из разных потоков)
  std::mutex write_mutex_;
};

} // namespace tcp_messenger

#endif // TCP_CLIENT_HPP
