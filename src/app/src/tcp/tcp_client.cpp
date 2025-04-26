// src/tcp/tcp_client.cpp
#include "tcp_client.hpp"
#include "common/json_util.h" // Предполагается, что json_util содержит хелперы для создания JSON
#include <iostream>
#include <boost/bind/bind.hpp>
#include <filesystem> // Для C++17 filesystem (если нужно для send_file_offer)
#include <boost/beast/core/detail/base64.hpp> // Для Base64 (если нужно для файлов)
#include <thread>
#include <chrono>

// TODO: Рассмотреть использование полноценной библиотеки логирования (например, spdlog)
// Helper для вывода логов клиента в stderr
void client_log(const std::string& level, const std::string& message) {
    // Выводим логи в stderr, чтобы не мешать JSON выводу для GUI в stdout
    std::cerr << "[TCPClient LOG " << level << "] " << message << std::endl;
}

namespace tcp_messenger {

using boost::asio::ip::tcp;
namespace fs = std::filesystem;
namespace base64 = boost::beast::detail::base64;

TCPClient::TCPClient(const std::string& server_host, unsigned short server_port)
    : server_host_(server_host), server_port_(server_port),
      io_context_(), socket_(io_context_), running_(false), connected_(false)
{
    client_log("INFO", "Initialized. Server: " + server_host_ + ":" + std::to_string(server_port_));
    // Инициализация коллбэков пустыми лямбдами
    on_register_result = [](bool, const std::string&){};
    on_login_result = [](bool, const std::string&){};
    on_message_received = [](const std::string&, const std::string&){};
    on_task_list_received = [](const json&){};
    on_task_notification = [](const json&){};
    on_server_error = [](const std::string&){};
    on_server_status = [](const std::string&){};
    on_connection_status_changed = [](bool){};
    // TODO: Добавить инициализацию коллбэков для передачи файлов
}

TCPClient::~TCPClient() {
    stop();
}

void TCPClient::start() {
    if (running_.exchange(true)) {
        client_log("WARNING", "Already started.");
        return;
    }
    client_log("INFO", "Starting...");
    io_context_.restart(); // Сбрасываем io_context перед запуском потока
    io_thread_ = std::thread([this]() { run_io_context(); });
    boost::asio::post(io_context_, [this]() { do_connect(); });
}

void TCPClient::stop() {
    if (!running_.exchange(false)) {
        return; // Уже остановлен
    }
    client_log("INFO", "Stopping...");
    // Используем post, чтобы операции закрытия выполнялись в потоке io_context
    boost::asio::post(io_context_, [this]() {
        if (socket_.is_open()) {
            boost::system::error_code ec;
            // Игнорируем ошибки при закрытии, т.к. останавливаемся
            socket_.shutdown(tcp::socket::shutdown_both, ec);
            socket_.close(ec);
        }
        // Не вызываем io_context.stop() здесь, она остановится сама,
        // когда не будет работы или будет остановлена извне.
    });

    if (io_thread_.joinable()) {
        io_thread_.join();
    }

    // Убеждаемся, что статус обновлен после остановки потока
    if (connected_.exchange(false)) {
        try {
            on_connection_status_changed(false);
        } catch (const std::exception& e) {
             client_log("ERROR", "Exception in on_connection_status_changed callback during stop: " + std::string(e.what()));
        }
    }
    client_log("INFO", "Stopped.");
}

void TCPClient::run_io_context() {
    client_log("INFO", "IO thread started.");
    try {
        io_context_.run(); // Блокирует до остановки или отсутствия работы
    } catch (const std::exception& e) {
        client_log("CRITICAL", "Exception in IO thread: " + std::string(e.what()));
    } catch (...) {
        client_log("CRITICAL", "Unknown exception in IO thread.");
    }
    client_log("INFO", "IO thread finished.");
    // Убедимся, что флаги сброшены и коллбэк вызван, если io_context завершился неожиданно
    running_ = false;
    if (connected_.exchange(false)) {
         try {
             on_connection_status_changed(false);
         } catch (const std::exception& e) {
              client_log("ERROR", "Exception in on_connection_status_changed callback during IO finish: " + std::string(e.what()));
         }
    }
}


void TCPClient::do_connect() {
    if (!running_) return;
    client_log("INFO", "Attempting to connect to " + server_host_ + ":" + std::to_string(server_port_) + "...");

    tcp::resolver resolver(io_context_);
    auto endpoints = resolver.resolve(server_host_, std::to_string(server_port_));

    boost::asio::async_connect(socket_, endpoints,
        [this](boost::system::error_code ec, tcp::endpoint /*endpoint*/) {
            if (!running_) return; // Остановка во время подключения

            if (!ec) {
                client_log("INFO", "Connected successfully.");
                if (!connected_.exchange(true)) { // Вызываем коллбэк только при смене статуса
                    on_connection_status_changed(true);
                }
                // Сбросить очередь записи при реконнекте? Зависит от требований.
                // { std::lock_guard<std::mutex> lock(write_mutex_); write_msgs_.clear(); }
                start_read();
                // Запускаем запись, если что-то осталось в очереди после дисконнекта
                boost::asio::post(io_context_, [this](){
                    std::lock_guard<std::mutex> lock(write_mutex_);
                    if (!write_msgs_.empty()) do_write();
                });
            } else {
                client_log("ERROR", "Connection failed: " + ec.message());
                if (connected_.exchange(false)) { // Вызываем коллбэк только при смене статуса
                    on_connection_status_changed(false);
                }
                // TODO: Реализовать более умную стратегию переподключения (exponential backoff)
                std::thread([this](){
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    if (running_) {
                        boost::asio::post(io_context_, [this]() { do_connect(); });
                    }
                }).detach();
            }
        });
}

void TCPClient::start_read() {
    boost::asio::async_read_until(socket_, read_buf_, '\n',
        // Используем boost::bind для передачи this и плейсхолдеров
        boost::bind(&TCPClient::handle_server_message, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
}

void TCPClient::handle_server_message(const boost::system::error_code& ec, std::size_t bytes_transferred) {
    (void)bytes_transferred; // Помечаем как неиспользуемый для компилятора
    if (!running_) return;

    if (!ec) {
        std::istream is(&read_buf_);
        std::string line;
        std::getline(is, line);

        if (!line.empty()) {
            // client_log("DEBUG", "Raw RECV: " + line); // Можно раскомментировать для глубокой отладки
            try {
                json data = json::parse(line);
                if (!data.contains("event") || !data["event"].is_string()) {
                    client_log("ERROR", "Received invalid JSON from server (missing/invalid event field).");
                    if(on_server_error) on_server_error("Received invalid JSON from server (missing/invalid event field)");
                } else {
                    std::string event = data["event"];
                    // Маршрутизация событий от сервера -> вызов соответствующих коллбэков
                    // Коллбэки должны быть потокобезопасными или вызываться в главном потоке GUI
                    if (event == "register_success") { if(on_register_result) on_register_result(true, "Registration successful."); }
                    else if (event == "login_success") { if(on_login_result) on_login_result(true, "Login successful."); }
                    else if (event == "message") { if (data.contains("from") && data.contains("text") && on_message_received) on_message_received(data["from"], data["text"]); }
                    else if (event == "task_list") { if(on_task_list_received) on_task_list_received(data); }
                    else if (event == "task_notification") { if(on_task_notification) on_task_notification(data); }
                    else if (event == "status") { if (data.contains("text") && on_server_status) on_server_status(data["text"]); }
                    else if (event == "error") {
                        std::string error_msg = data.value("text", "Unknown server error");
                        client_log("ERROR", "Server Error Reported: " + error_msg);
                        if(on_server_error) on_server_error(error_msg);
                        if (error_msg.find("Login failed") != std::string::npos) { if(on_login_result) on_login_result(false, error_msg); }
                        else if (error_msg.find("Registration failed") != std::string::npos) { if(on_register_result) on_register_result(false, error_msg); }
                    }
                    // TODO: Добавить обработку событий file_offer, file_accept, file_data и т.д.
                    else { client_log("WARNING", "Received unhandled event '" + event + "' from server."); }
                }
            } catch (json::parse_error& e) {
                client_log("ERROR", "Failed to parse JSON from server: " + std::string(e.what()) + ", raw: [" + line + "]");
                if(on_server_error) on_server_error("Received invalid JSON from server: " + std::string(e.what()));
            }
        }
        // Продолжаем чтение, если все еще работаем и сокет открыт
        if (running_ && socket_.is_open()) {
             start_read();
        }

    } else if (ec == boost::asio::error::eof || ec == boost::asio::error::connection_reset) {
        close_connection("Connection closed by peer (EOF/Reset).");
    } else if (ec == boost::asio::error::operation_aborted) {
        client_log("INFO", "Read operation aborted (likely client stopping).");
        // Не вызываем close_connection, т.к. остановка инициирована извне
    } else {
         close_connection("Read error: " + ec.message());
    }
}


// Потокобезопасный метод для отправки JSON (вызывается из любого потока)
void TCPClient::send_json(const json& data) {
    if (!running_) {
         client_log("WARNING", "Cannot send JSON: Client not running.");
         // TODO: Уведомить GUI об ошибке?
         return;
    }
     if (!connected_) {
          client_log("WARNING", "Cannot send JSON: Client not connected.");
          // TODO: Уведомить GUI об ошибке? Или кешировать сообщение?
          return;
     }

    std::string data_str = data.dump() + "\n";
    client_log("DEBUG", "Attempting to queue JSON for sending: " + data.dump());

    // Используем post для безопасной передачи задачи в поток io_context
    boost::asio::post(io_context_, [this, data_str, data_copy = data]() {
         client_log("DEBUG", "Executing post lambda in send_json for event: " + data_copy.value("command", data_copy.value("event", "N/A"))); // Используем копию для лога
         bool start_write = false;
        {
           std::lock_guard<std::mutex> lock(write_mutex_);
           start_write = write_msgs_.empty();            // очередь была пуста?
           write_msgs_.push_back(std::move(data_str));
        }  
         if (start_write) {
             // Запускаем do_write только если очередь была пуста
             client_log("DEBUG", "Write not in progress, calling do_write().");
             do_write(); // do_write сама возьмет мьютекс позже
         } else {
             client_log("DEBUG", "Write already in progress, message queued.");
         }
     });
}


// Запускает асинхронную запись (вызывается только из потока io_context)
void TCPClient::do_write() {
    // Мьютекс УЖЕ должен быть взят вызывающей функцией, если мы здесь не из коллбэка async_write
    // Но для безопасности и ясности возьмем его здесь, т.к. коллбэк тоже вызывает do_write
    std::lock_guard<std::mutex> lock(write_mutex_);

    if (write_msgs_.empty() || !socket_.is_open()) {
        return; // Нечего отправлять или сокет закрыт
    }

    client_log("DEBUG", "Starting async_write for: " + write_msgs_.front().substr(0, 100) + "...");

    // Запускаем асинхронную запись первого сообщения из очереди
    boost::asio::async_write(socket_, boost::asio::buffer(write_msgs_.front()),
        [this](boost::system::error_code ec, std::size_t length) {
            // Этот коллбэк выполняется в потоке io_context

            if (!running_) return; // Проверка на случай остановки во время записи

            // Берем мьютекс для безопасной работы с очередью и логгирования
            std::lock_guard<std::mutex> lock(write_mutex_);
            std::string message_prefix = write_msgs_.empty() ? "N/A" : write_msgs_.front().substr(0, 50); // Для лога ошибки

            if (!ec) {
                // Запись успешна
                client_log("DEBUG", "async_write completed successfully. Bytes: " + std::to_string(length) + ". Msg prefix: " + message_prefix + "...");
                if (!write_msgs_.empty()) {
                    write_msgs_.pop_front(); // Удаляем отправленное сообщение
                    if (!write_msgs_.empty()) {
                        // Если есть еще сообщения, запускаем следующую запись
                        client_log("DEBUG", "More messages in queue, calling do_write() again.");
                        do_write(); // Рекурсивный вызов (безопасно, т.к. асинхронный)
                    } else {
                        client_log("DEBUG", "Write queue is now empty.");
                    }
                } else {
                     client_log("WARNING", "Write queue was empty after successful write callback?");
                }
            } else if (ec == boost::asio::error::operation_aborted) {
                 client_log("INFO", "Write operation aborted (likely client stopping).");
                 write_msgs_.clear(); // Очищаем очередь, т.к. останавливаемся
            } else {
                // Ошибка записи
                client_log("ERROR", "async_write failed: " + ec.message() + ". Msg prefix: " + message_prefix + "...");
                write_msgs_.clear(); // Очищаем очередь при ошибке

                // Закрываем соединение асинхронно, чтобы избежать рекурсивных вызовов close_connection
                // Используем post для выполнения в io_context
                 boost::asio::post(io_context_, [this, reason = "Write error: " + ec.message()](){
                     close_connection(reason);
                 });
            }
        });
}


// Закрывает соединение и инициирует переподключение (вызывается из потока io_context)
void TCPClient::close_connection(const std::string& reason) {
    if (!connected_.exchange(false)) {
        // Уже обрабатываем дисконнект или уже не подключены
        return;
    }
    client_log("ERROR", "Closing connection: " + reason);
    on_connection_status_changed(false); // Уведомляем GUI

    // Закрываем сокет (мы уже в io_context потоке, можно делать напрямую)
    if(socket_.is_open()){
        boost::system::error_code close_ec;
        socket_.shutdown(tcp::socket::shutdown_both, close_ec); // Игнорируем ошибки shutdown
        socket_.close(close_ec); // Игнорируем ошибки close
    }

    // Попытка переподключения, если клиент еще должен работать
    if (running_) {
         client_log("INFO", "Attempting reconnect after disconnect...");
          // Запускаем do_connect с задержкой через таймер Asio, чтобы не блокировать io_context
          auto timer = std::make_shared<boost::asio::steady_timer>(io_context_);
          timer->expires_after(std::chrono::seconds(5));
          timer->async_wait([this, timer](const boost::system::error_code& ec) {
              if (!ec && running_) { // Если таймер не отменен и клиент все еще работает
                  do_connect();
              }
          });
    }
}


// --- Публичные методы API (вызываются извне, например, из GUI потока) ---

void TCPClient::register_user(const std::string& username, const std::string& password) {
    // TODO: Добавить базовую валидацию username/password перед отправкой
    json reg_msg = common::base_event("register"); // Используем хелпер, если есть
    reg_msg["command"] = "register"; // Или явно задаем команду
    reg_msg["username"] = username;
    reg_msg["password"] = password; // TODO: Передавать пароль безопасно (TLS)!
    send_json(reg_msg);
}

void TCPClient::login(const std::string& username, const std::string& password) {
    json login_msg;
    login_msg["command"] = "login";
    login_msg["username"] = username;
    login_msg["password"] = password; // TODO: Передавать пароль безопасно (TLS)!
    // current_username_ лучше устанавливать после получения login_success от сервера
    // current_username_ = username; // Пока уберем
    send_json(login_msg);
}

void TCPClient::send_message(const std::string &to, const std::string &text) {
     // TODO: Возможно, стоит проверять current_username_ здесь, чтобы GUI знал, залогинен ли пользователь
     json msg;
     msg["command"] = "send_message";
     msg["to"] = to;
     msg["text"] = text;
     // Сервер использует имя пользователя из аутентифицированной сессии как 'from'
    send_json(msg);
}

void TCPClient::add_task(const std::string& task_name, const std::string& description, const std::string& trigger_time, int notify_offset) {
     json task_msg;
     task_msg["command"] = "add_task";
     task_msg["task_name"] = task_name;
     task_msg["description"] = description;
     task_msg["trigger_time"] = trigger_time; // Ожидаемый формат: "DD.MM.YYYY HH:MM"
     task_msg["notify_offset"] = notify_offset;
     send_json(task_msg);
}

void TCPClient::request_task_list() {
     json req_msg;
     req_msg["command"] = "list_tasks";
     send_json(req_msg);
}

// TODO: Реализовать методы для начала передачи файла (send_file_offer)
void TCPClient::send_file_offer(const std::string& to, const std::string& file_path_str) {
     client_log("WARNING", "send_file_offer is not fully implemented yet.");
     // 1. Проверить существование файла, размер (fs::path, fs::exists, fs::file_size)
     // 2. Сформировать JSON file_offer с from, to, file_name, file_size
     // 3. Вызвать send_json(offer_msg)
     // 4. Установить состояние ожидания ответа (file_accept/file_reject) для этого файла/получателя
}

// TODO: Реализовать обработку file_accept/file_reject от сервера в handle_server_message
// TODO: Реализовать чтение файла чанками и отправку file_data (Base64) после получения file_accept
// TODO: Реализовать прием file_offer/file_data от других клиентов

} // namespace tcp_messenger
