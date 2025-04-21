#include "tcp_client.hpp"
#include "common/json_util.h" // Для создания JSON
#include <iostream>
#include <boost/bind/bind.hpp> // Для boost::bind
#include <boost/beast/core/detail/base64.hpp> // Для Base64
#include <filesystem> // Для работы с путями файлов C++17

namespace tcp_messenger {

using boost::asio::ip::tcp;
namespace fs = std::filesystem;
namespace base64 = boost::beast::detail::base64; // Псевдоним

TCPClient::TCPClient(const std::string& server_host, unsigned short server_port)
    : server_host_(server_host),
      server_port_(server_port),
      io_context_(),
      socket_(io_context_),
      running_(false),
      connected_(false),
      // Инициализируем коллбэки в списке инициализации
      on_register_result([](bool, const std::string&){}),
      on_login_result([](bool, const std::string&){}),
      on_message_received([](const std::string&, const std::string&){}),
      on_task_list_received([](const json&){}),
      on_task_notification([](const json&){}),
      on_server_error([](const std::string&){}),
      on_server_status([](const std::string&){}),
      on_connection_status_changed([](bool){})
{
     std::cout << "[TCPClient] Initialized. Server: " << server_host_ << ":" << server_port_ << std::endl;
     // Тело конструктора теперь пустое или содержит другую логику, если нужна
}

TCPClient::~TCPClient() {
    stop(); // Убедимся, что все остановлено
}

void TCPClient::start() {
    if (running_.exchange(true)) {
         std::cerr << "[TCPClient] Already started." << std::endl;
        return; // Уже запущен
    }
    std::cout << "[TCPClient] Starting..." << std::endl;
    // Запускаем io_context в отдельном потоке
    io_thread_ = std::thread([this]() { run_io_context(); });
    // Запускаем попытку подключения из потока io_context
    boost::asio::post(io_context_, [this]() { do_connect(); });
}

void TCPClient::stop() {
    if (!running_.exchange(false)) {
        return; // Уже остановлен
    }
     std::cout << "[TCPClient] Stopping..." << std::endl;
    // Используем post, чтобы операции выполнялись в потоке io_context
    boost::asio::post(io_context_, [this]() {
        if (socket_.is_open()) {
            boost::system::error_code ec;
            socket_.shutdown(tcp::socket::shutdown_both, ec);
            socket_.close(ec);
        }
        // io_context остановится сам, когда не останется работы
    });

    // Дожидаемся завершения потока io_context
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
    connected_ = false;
    on_connection_status_changed(false);
    std::cout << "[TCPClient] Stopped." << std::endl;
}

void TCPClient::run_io_context() {
     std::cout << "[TCPClient] IO thread started." << std::endl;
     try {
        io_context_.run(); // Блокирует до остановки
     } catch (const std::exception& e) {
          std::cerr << "[TCPClient] Exception in IO thread: " << e.what() << std::endl;
     } catch (...) {
          std::cerr << "[TCPClient] Unknown exception in IO thread." << std::endl;
     }
     std::cout << "[TCPClient] IO thread finished." << std::endl;
     running_ = false; // Убедимся, что флаг сброшен
     if (connected_) {
         connected_ = false;
         on_connection_status_changed(false);
     }
}


void TCPClient::do_connect() {
    if (!running_) return; // Не пытаемся подключиться, если остановлены
    std::cout << "[TCPClient] Attempting to connect to " << server_host_ << ":" << server_port_ << "..." << std::endl;

    tcp::resolver resolver(io_context_);
    auto endpoints = resolver.resolve(server_host_, std::to_string(server_port_));

    boost::asio::async_connect(socket_, endpoints,
        [this](boost::system::error_code ec, tcp::endpoint /*endpoint*/) {
            if (!running_) return; // Остановка во время подключения

            if (!ec) {
                std::cout << "[TCPClient] Connected successfully." << std::endl;
                connected_ = true;
                on_connection_status_changed(true);
                start_read(); // Начинаем читать с сервера
                // do_write(); // Запускаем запись, если есть что отправлять (например, логин после реконнекта)
            } else {
                std::cerr << "[TCPClient] Connection failed: " << ec.message() << std::endl;
                connected_ = false;
                on_connection_status_changed(false);
                // Попытка переподключения через некоторое время
                // Важно: не создавайте бесконечный цикл без задержки
                std::this_thread::sleep_for(std::chrono::seconds(5)); // Пауза перед повторной попыткой
                if (running_) { // Проверяем еще раз перед рекурсивным вызовом
                     do_connect(); // Рекурсивный вызов для переподключения
                }
            }
        });
}

void TCPClient::start_read() {
    // std::cout << "[TCPClient DEBUG] start_read called." << std::endl;
    boost::asio::async_read_until(socket_, read_buf_, '\n',
        boost::bind(&TCPClient::handle_server_message, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
}

void TCPClient::handle_server_message(const boost::system::error_code& ec, std::size_t bytes_transferred) {
    // std::cout << "[TCPClient DEBUG] handle_server_message. EC: " << ec.message() << ", Bytes: " << bytes_transferred << std::endl;
    if (!running_) return; // Остановлен

    if (!ec) {
        std::istream is(&read_buf_);
        std::string line;
        std::getline(is, line);

        if (!line.empty()) {
            // std::cout << "[TCPClient RECV] " << line << std::endl; // Лог полученного JSON
            try {
                json data = json::parse(line);
                if (!data.contains("event") || !data["event"].is_string()) {
                     std::cerr << "[TCPClient] Received invalid JSON from server (missing event field)." << std::endl;
                    if(on_server_error) on_server_error("Received invalid JSON from server");
                    // Продолжаем читать? Или закрыть соединение?
                } else {
                    std::string event = data["event"];
                    // Маршрутизация событий от сервера
                    if (event == "register_success") {
                        if(on_register_result) on_register_result(true, "Registration successful.");
                    } else if (event == "login_success") {
                         // current_username_ устанавливается в login() методе
                         if(on_login_result) on_login_result(true, "Login successful.");
                    } else if (event == "message") {
                        if (data.contains("from") && data.contains("text")) {
                            if(on_message_received) on_message_received(data["from"], data["text"]);
                        }
                    } else if (event == "task_list") {
                         if(on_task_list_received) on_task_list_received(data); // Передаем весь JSON
                    } else if (event == "task_notification") {
                         if(on_task_notification) on_task_notification(data); // Передаем весь JSON
                    } else if (event == "status") {
                        if (data.contains("text")) {
                             if(on_server_status) on_server_status(data["text"]);
                        }
                    } else if (event == "error") {
                        std::string error_msg = data.value("text", "Unknown server error");
                        std::cerr << "[TCPClient Server Error] " << error_msg << std::endl;
                         if(on_server_error) on_server_error(error_msg);
                        // Смотрим, является ли ошибка фатальной для логина/регистрации
                        if (error_msg.find("Login failed") != std::string::npos) {
                             if(on_login_result) on_login_result(false, error_msg);
                        } else if (error_msg.find("Registration failed") != std::string::npos) {
                             if(on_register_result) on_register_result(false, error_msg);
                        }
                    }
                    // Добавить обработку событий file_offer, file_accept и т.д.
                    else {
                        std::cout << "[TCPClient] Received unhandled event '" << event << "' from server." << std::endl;
                    }
                }
            } catch (json::parse_error& e) {
                std::cerr << "[TCPClient] Failed to parse JSON from server: " << e.what() << ", raw: [" << line << "]" << std::endl;
                if(on_server_error) on_server_error("Received invalid JSON from server");
            }
        }
        // Продолжаем читать
        if (running_ && socket_.is_open()) {
             start_read();
        }

    } else if (ec == boost::asio::error::eof || ec == boost::asio::error::connection_reset) {
        close_connection("Connection closed by peer (EOF/Reset).");
    } else if (ec == boost::asio::error::operation_aborted) {
        // Ничего не делаем, операция отменена извне (при остановке)
        std::cout << "[TCPClient] Read operation aborted." << std::endl;
    } else {
         close_connection("Read error: " + ec.message());
    }
}


// Добавляет JSON строку в очередь и запускает отправку, если не идет
void TCPClient::send_json(const json& data) {
    if (!running_ || !connected_) {
         std::cerr << "[TCPClient] Cannot send data: Not running or not connected." << std::endl;
         // Можно кешировать сообщения для отправки после реконнекта?
         return;
    }
    std::string data_str = data.dump() + "\n";
    // std::cout << "[TCPClient SEND] " << data_str.substr(0, 100) << "..." << std::endl;

    boost::asio::post(io_context_, [this, data_str]() {
         std::lock_guard<std::mutex> lock(write_mutex_); // Защищаем очередь
         bool write_in_progress = !write_msgs_.empty();
         write_msgs_.push_back(std::move(data_str));
         if (!write_in_progress) {
             do_write();
         }
     });
}


void TCPClient::do_write() {
    // Вызывается только из io_context потока (из send_json через post или из самого себя)
    std::lock_guard<std::mutex> lock(write_mutex_); // Защищаем доступ к очереди
    if (write_msgs_.empty() || !socket_.is_open()) {
        return; // Нечего отправлять или сокет закрыт
    }

    boost::asio::async_write(socket_, boost::asio::buffer(write_msgs_.front()),
        [this](boost::system::error_code ec, std::size_t /*length*/) {
            if (!running_) return;

            std::lock_guard<std::mutex> lock(write_mutex_); // Защищаем очередь
            if (!ec) {
                write_msgs_.pop_front(); // Удаляем успешно отправленное
                if (!write_msgs_.empty()) {
                    do_write(); // Отправляем следующее, если есть
                }
            } else if (ec == boost::asio::error::operation_aborted) {
                 std::cout << "[TCPClient] Write operation aborted." << std::endl;
                 write_msgs_.clear(); // Очищаем очередь
            } else {
                 write_msgs_.clear(); // Очищаем очередь при ошибке
                 close_connection("Write error: " + ec.message());
            }
        });
}


void TCPClient::close_connection(const std::string& reason) {
    if (!connected_.exchange(false)) { // Убедимся, что закрываем только один раз
        return;
    }
     std::cerr << "[TCPClient] Closing connection: " << reason << std::endl;
     on_connection_status_changed(false);

     // Закрываем сокет из io_context потока
     boost::asio::post(io_context_, [this]() {
         if(socket_.is_open()){
             boost::system::error_code ec;
             socket_.shutdown(tcp::socket::shutdown_both, ec);
             socket_.close(ec);
         }
         // Попытка переподключения (если клиент еще работает)
         if (running_) {
              std::cout << "[TCPClient] Attempting reconnect..." << std::endl;
              std::this_thread::sleep_for(std::chrono::seconds(5));
              if(running_) do_connect();
         }
     });
}


// --- Публичные методы для отправки команд ---

void TCPClient::register_user(const std::string& username, const std::string& password) {
    json reg_msg = common::base_event("register");
    reg_msg["username"] = username;
    reg_msg["password"] = password; // Пароль передается открытым текстом! Нужен TLS!
    send_json(reg_msg);
}

void TCPClient::login(const std::string& username, const std::string& password) {
    json login_msg = common::base_event("login");
    login_msg["username"] = username;
    login_msg["password"] = password; // Пароль передается открытым текстом! Нужен TLS!
    // Сохраняем имя пользователя локально ДО получения ответа от сервера,
    // чтобы знать, кто мы, если логин пройдет успешно.
    // В реальном приложении лучше дождаться login_success.
    current_username_ = username;
    send_json(login_msg);
}

void TCPClient::send_message(const std::string &to, const std::string &text) {
     if (current_username_.empty()){
          std::cerr << "[TCPClient] Cannot send message: Not logged in." << std::endl;
          if(on_server_error) on_server_error("Cannot send message: Not logged in.");
          return;
     }
     // Используем common::message_event, который сам выставит from/to/text/event
     json msg = common::message_event(current_username_, to, text);
     // Поле 'from' здесь не обязательно, сервер все равно возьмет из сессии
     // Но для консистентности можно оставить
     // msg["from"] = current_username_; // Перезапишем на всякий случай
    send_json(msg);
}

void TCPClient::add_task(const std::string& task_name, const std::string& description, const std::string& trigger_time, int notify_offset) {
     if (current_username_.empty()){
          std::cerr << "[TCPClient] Cannot add task: Not logged in." << std::endl;
           if(on_server_error) on_server_error("Cannot add task: Not logged in.");
          return;
     }
     json task_msg = common::base_event("add_task");
     task_msg["username"] = current_username_; // Можно не слать, сервер знает
     task_msg["task_name"] = task_name;
     task_msg["description"] = description;
     task_msg["trigger_time"] = trigger_time; // Формат DD.MM.YYYY HH:MM
     task_msg["notify_offset"] = notify_offset;
     send_json(task_msg);
}

void TCPClient::request_task_list() {
      if (current_username_.empty()){
          std::cerr << "[TCPClient] Cannot request tasks: Not logged in." << std::endl;
          if(on_server_error) on_server_error("Cannot request tasks: Not logged in.");
          return;
      }
     json req_msg = common::base_event("list_tasks");
     req_msg["username"] = current_username_; // Можно не слать
     send_json(req_msg);
}

// --- Передача файла (начало) ---
void TCPClient::send_file_offer(const std::string& to, const std::string& file_path_str) {
     if (current_username_.empty()){
          std::cerr << "[TCPClient] Cannot send file: Not logged in." << std::endl;
           if(on_server_error) on_server_error("Cannot send file: Not logged in.");
          return;
     }

     fs::path file_path(file_path_str);
     if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
          std::cerr << "[TCPClient] File not found or is not a regular file: " << file_path_str << std::endl;
          if(on_server_error) on_server_error("File not found: " + file_path.filename().string());
          return;
     }

     uintmax_t file_size = fs::file_size(file_path);
     std::string file_name = file_path.filename().string();

     // Ограничим размер файла для простоты (например, 10 МБ)
     const uintmax_t MAX_FILE_SIZE = 10 * 1024 * 1024;
     if (file_size > MAX_FILE_SIZE) {
          std::cerr << "[TCPClient] File is too large (max " << MAX_FILE_SIZE / (1024*1024) << " MB): " << file_name << std::endl;
           if(on_server_error) on_server_error("File is too large: " + file_name);
          return;
     }
      if (file_size == 0) {
           std::cerr << "[TCPClient] Cannot send empty file: " << file_name << std::endl;
           if(on_server_error) on_server_error("Cannot send empty file: " + file_name);
          return;
      }

     json offer_msg = common::base_event("file_offer");
     offer_msg["from"] = current_username_;
     offer_msg["to"] = to;
     offer_msg["file_name"] = file_name;
     offer_msg["file_size"] = file_size;

     std::cout << "[TCPClient] Sending file offer for '" << file_name << "' (" << file_size << " bytes) to '" << to << "'." << std::endl;
     send_json(offer_msg);

     // TODO: Сохранить состояние ожидания file_accept/file_reject
     // TODO: При получении file_accept, начать читать файл и отправлять file_data чанками (Base64)
}


// TODO: Реализовать обработку file_accept, file_reject, file_data на клиенте
// TODO: Реализовать отправку file_data чанками в Base64


} // namespace tcp_messenger
