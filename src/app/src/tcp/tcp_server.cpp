// src/tcp/tcp_server.cpp
#include "tcp_server.hpp"
#include <boost/asio/strand.hpp>
#include <boost/beast/core/detail/base64.hpp>
#include <iostream>
#include <istream>
#include <memory>
#include <deque>
#include <optional>

namespace tcp_messenger {

using boost::asio::ip::tcp;
using json = nlohmann::json;
namespace base64 = boost::beast::detail::base64;



TCPServer::Session::Session(TCPServer& server, tcp::socket socket)
    : server_(server), socket_(std::move(socket)), authenticated_(false), remote_ep_str_("unknown")
{
    try {
        auto ep = socket_.remote_endpoint();
        remote_ep_str_ = ep.address().to_string() + ":" + std::to_string(ep.port());
    } catch (const std::exception& e) {
        std::cerr << "[" << get_log_timestamp() << "][Session DEBUG] Error getting remote endpoint on creation: " << e.what() << std::endl;
    }
     std::cout << "[" << get_log_timestamp() << "][Session] Created for endpoint: " << remote_ep_str_ << std::endl;
}

TCPServer::Session::~Session() {
     std::cout << "[" << get_log_timestamp() << "][Session] Destroyed for user: '" << get_session_id() << "'" << std::endl;
}

void TCPServer::Session::start() {
     std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Starting..." << std::endl;
     // Используем strand для сокета, чтобы гарантировать последовательную обработку read/write
     // и избежать гонок данных, если операции вызываются из разных потоков (например, write из другого потока)
     // boost::asio::dispatch(socket_.get_executor(), boost::bind(&Session::do_read, shared_from_this()));
     // Пока io_context работает в одном потоке, strand не строго обязателен, но это хорошая практика.
     do_read();
}

// Отправка данных: добавляем в очередь и запускаем do_write, если не запущен
void TCPServer::Session::send(const json& data) {
    // Конвертируем JSON в строку и добавляем разделитель
    std::string data_str = data.dump() + "\n";

    // std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Queuing send: [" << data_str.substr(0, 100) << "...]" << std::endl;
    // Используем post для безопасного добавления в очередь из любого потока
    boost::asio::post(socket_.get_executor(),
        [self = shared_from_this(), data_str]() {
            bool write_in_progress = !self->write_msgs_.empty();
            self->write_msgs_.push_back(std::move(data_str));
            if (!write_in_progress) {
                self->do_write();
            }
        });
}

void TCPServer::Session::close_socket() {
     // Используем post чтобы выполнить закрытие в потоке io_context
     boost::asio::post(socket_.get_executor(), [self = shared_from_this()]() {
        if (self->socket_.is_open()) {
             std::cout << "[" << get_log_timestamp() << "][Session:" << self->get_session_id() << "] Closing socket." << std::endl;
             boost::system::error_code ec_shutdown, ec_close;
             self->socket_.shutdown(tcp::socket::shutdown_both, ec_shutdown);
             // Игнорируем not_connected при shutdown
             if (ec_shutdown && ec_shutdown != boost::asio::error::not_connected) {
                  std::cerr << "[" << get_log_timestamp() << "][Session:" << self->get_session_id() << "] Shutdown error: " << ec_shutdown.message() << std::endl;
             }
             self->socket_.close(ec_close);
             if (ec_close) {
                  std::cerr << "[" << get_log_timestamp() << "][Session:" << self->get_session_id() << "] Close error: " << ec_close.message() << std::endl;
             }
         }
     });
}

std::string TCPServer::Session::get_username() const { return username_; }
bool TCPServer::Session::is_authenticated() const { return authenticated_; }

std::pair<std::string, unsigned short> TCPServer::Session::get_remote_address() const {
    boost::system::error_code ec;
    tcp::endpoint endpoint = socket_.remote_endpoint(ec);
    if (ec) { return {"unknown", 0}; }
    return {endpoint.address().to_string(), endpoint.port()};
}

std::string TCPServer::Session::get_session_id(bool prefer_username) const {
    if (authenticated_ && !username_.empty()) return username_;
    if (prefer_username && !username_.empty()) return username_; // На случай если вызвали до аутентификации
    return remote_ep_str_;
}

void TCPServer::Session::set_authenticated_user(const std::string& name) {
     // Вызывается из обработчика login в TCPServer, уже в потоке io_context
     std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id(true) << "] Authenticated as: '" << name << "'" << std::endl;
     username_ = name;
     authenticated_ = true;
}


void TCPServer::Session::do_read() {
    // std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] do_read() called." << std::endl;
    auto self = shared_from_this();
    boost::asio::async_read_until(socket_, buffer_, '\n',
        // Убираем имя неиспользуемого параметра bytes_transferred
        [this, self](boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
            // std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] async_read_until completed. EC: " << ec.message() << ", Bytes: " << bytes_transferred << std::endl;

            if (!ec) {
                std::istream is(&buffer_);
                std::string line;
                std::getline(is, line); // Читаем до \n

                if (!line.empty()) {
                    // Передаем необработанную строку в сервер для парсинга JSON
                    server_.handle_message(line, self);
                }
                // Продолжаем чтение, если сокет все еще открыт
                if (socket_.is_open()) {
                    do_read();
                } else {
                     std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Socket closed during read processing." << std::endl;
                     server_.remove_client(username_, self); // Убедимся, что клиент удален
                }

            } else if (ec == boost::asio::error::eof || ec == boost::asio::error::connection_reset) {
                std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Connection closed by peer (EOF/Reset)." << std::endl;
                server_.remove_client(username_, self);
            } else if (ec == boost::asio::error::operation_aborted) {
                std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Read operation aborted." << std::endl;
                // Не вызываем remove_client, т.к. операция отменена извне (вероятно, при stop())
            } else {
                std::cerr << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Read error: " << ec.message() << std::endl;
                server_.remove_client(username_, self);
            }
        });
}

void TCPServer::Session::do_write() {
    // std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] do_write() called. Queue size: " << write_msgs_.size() << std::endl;
    if (write_msgs_.empty()) {
        return; // Нечего отправлять
    }

    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(write_msgs_.front()),
        // Убираем имя неиспользуемого параметра length
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
             // std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] async_write completed. EC: " << ec.message() << ", Bytes: " << length << std::endl;

            if (!ec) {
                // std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Sent " << length << " bytes." << std::endl;
                write_msgs_.pop_front(); // Удаляем успешно отправленное сообщение
                if (!write_msgs_.empty()) {
                    do_write(); // Запускаем отправку следующего, если есть
                }
            } else if (ec == boost::asio::error::operation_aborted) {
                 std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Write operation aborted." << std::endl;
                 // Очищаем очередь, т.к. соединение закрывается
                 write_msgs_.clear();
                 // Не вызываем remove_client здесь
            } else {
                std::cerr << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Write error: " << ec.message() << std::endl;
                // Очищаем очередь и закрываем сокет/удаляем клиента
                 write_msgs_.clear();
                server_.remove_client(username_, self);
                // Неявно закроем сокет через remove_client -> close_socket
            }
        });
}

// --- Реализация TCPServer ---

TCPServer::TCPServer(boost::asio::io_context& io_context,
                     const std::string& db_path,
                     unsigned short app_port,
                     unsigned short metrics_port)
    : io_context_(io_context),
      acceptor_(io_context, tcp::endpoint(tcp::v4(), app_port)),
      db_manager_(db_path), // Может бросить исключение
      exposer_("0.0.0.0:" + std::to_string(metrics_port)),
      registry_(std::make_shared<prometheus::Registry>()),
      cleanup_timer_(io_context) // Инициализируем таймер очистки
{
    // Инициализация Prometheus (как было)
    try {
        messages_total_counter_ = &prometheus::BuildCounter()
              .Name("messenger_messages_total")
              .Help("Total processed messages (type=message) by the server")
              .Register(*registry_)
              .Add({}); // Нет меток

        active_connections_gauge_ = &prometheus::BuildGauge()
              .Name("messenger_active_connections")
              .Help("Current authenticated TCP connections")
              .Register(*registry_)
              .Add({});

        exposer_.RegisterCollectable(registry_);
        active_connections_gauge_->Set(0); // Начальное значение

        std::cout << "[" << get_log_timestamp() << "][Server] Listening on TCP port " << app_port << std::endl;
        std::cout << "[" << get_log_timestamp() << "][Server] Exposing metrics on port " << metrics_port << std::endl;
        std::cout << "[" << get_log_timestamp() << "][Server] Database path: " << db_path << std::endl;

    } catch (const std::exception& e) {
         std::cerr << "[" << get_log_timestamp() << "][Server] CRITICAL ERROR during Prometheus setup: " << e.what() << std::endl;
         throw;
    }
}

void TCPServer::start() {
    std::cout << "[" << get_log_timestamp() << "][Server] Starting..." << std::endl;
    try {
        load_and_schedule_tasks(); // Загружаем и планируем задачи из БД
        start_cleanup_timer(); // Запускаем таймер очистки
        do_accept(); // Начинаем принимать соединения
        std::cout << "[" << get_log_timestamp() << "][Server] Started successfully." << std::endl;
    } catch (const std::exception& e) {
         std::cerr << "[" << get_log_timestamp() << "][Server] CRITICAL ERROR during startup: " << e.what() << std::endl;
         // Возможно, стоит остановить io_context или предпринять другие действия
         throw; // Перебрасываем, чтобы main мог завершиться
    }
}

void TCPServer::stop() {
    std::cout << "[" << get_log_timestamp() << "][Server] Gracefully stopping server..." << std::endl;
    boost::system::error_code ec; // Для acceptor_.close()

    // 1. Отменяем таймер очистки
    std::cout << "[" << get_log_timestamp() << "][Server] Cancelling cleanup timer..." << std::endl;
    // Исправлено: cancel() не принимает error_code
    cleanup_timer_.cancel();


    // 2. Отменяем все таймеры задач
    {
        std::lock_guard<std::mutex> lock(task_timers_mutex_);
        std::cout << "[" << get_log_timestamp() << "][Server] Cancelling " << task_timers_.size() << " task timer(s)..." << std::endl;
        for (auto const& [id, timer_ptr] : task_timers_) {
            // Исправлено: cancel() не принимает error_code
            std::size_t cancelled_count = timer_ptr->cancel();
             if (cancelled_count > 0) {
                  // Можно добавить лог, если нужно
                  // std::cout << "[" << get_log_timestamp() << "][Server] Cancelled timer for task " << id << std::endl;
             }
        }
        task_timers_.clear();
    }


    // 3. Прекращаем принимать новые соединения
    acceptor_.close(ec); // acceptor_.close() принимает error_code
    if (ec) { std::cerr << "[" << get_log_timestamp() << "][Server] Error closing acceptor: " << ec.message() << std::endl; }
    else { std::cout << "[" << get_log_timestamp() << "][Server] Acceptor closed." << std::endl; }


    // 4. Закрываем все активные клиентские сессии
    // Копируем указатели, чтобы итерация не инвалидировалась при удалении из карты в remove_client
    std::vector<std::shared_ptr<Session>> sessions_to_close;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        std::cout << "[" << get_log_timestamp() << "][Server] Closing " << clients_.size() << " active client connection(s)..." << std::endl;
        for (auto const& [name, session_ptr] : clients_) {
            sessions_to_close.push_back(session_ptr);
        }
         // Не очищаем clients_ здесь, remove_client сделает это
    }

    for(auto& session_ptr : sessions_to_close) {
        session_ptr->close_socket(); // Закрываем сокеты (асинхронно)
    }
    // Даем io_context немного времени обработать закрытия (опционально)
    // io_context_.poll();

    // Очищаем карту клиентов (на случай, если remove_client не успел отработать для всех)
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.clear();
    }


    // 5. Сбрасываем метрики
    active_connections_gauge_->Set(0);

    std::cout << "[" << get_log_timestamp() << "][Server] Stop sequence finished." << std::endl;
    // io_context_.stop() должен быть вызван извне (например, в main по SIGINT/SIGTERM)
}


void TCPServer::do_accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                // Успешно приняли соединение
                try {
                     std::string remote_ip = socket.remote_endpoint().address().to_string();
                     unsigned short remote_port = socket.remote_endpoint().port();
                     std::cout << "[" << get_log_timestamp() << "][Server] Accepted connection from "
                               << remote_ip << ":" << remote_port << std::endl;

                    // Создаем сессию для нового клиента
                    auto new_session = std::make_shared<Session>(*this, std::move(socket));
                    new_session->start(); // Запускаем чтение
                } catch (const std::bad_alloc& ba) {
                    std::cerr << "[" << get_log_timestamp() << "][Server] CRITICAL: Failed to allocate memory for new session!" << std::endl;
                    boost::system::error_code close_ec; socket.close(close_ec);
                } catch (const std::exception& e) {
                     std::cerr << "[" << get_log_timestamp() << "][Server] Error creating session: " << e.what() << std::endl;
                     boost::system::error_code close_ec; socket.close(close_ec);
                }
            } else if (ec == boost::asio::error::operation_aborted) {
                std::cout << "[" << get_log_timestamp() << "][Server] Accept operation aborted (server stopping?)." << std::endl;
                return; // Не продолжаем цикл
            } else {
                std::cerr << "[" << get_log_timestamp() << "][Server] Accept error: " << ec.message() << std::endl;
                // Можно добавить задержку перед повторной попыткой при некоторых ошибках (например, too many open files)
            }

            // Продолжаем слушать, если acceptor еще открыт
            if (acceptor_.is_open()) {
                 do_accept();
            } else {
                 std::cout << "[" << get_log_timestamp() << "][Server] Acceptor closed, stopping accept loop." << std::endl;
            }
        });
}

// Основной обработчик сообщений (парсит JSON и вызывает нужный хендлер)
void TCPServer::handle_message(const std::string& raw_message, std::shared_ptr<Session> session) {
    // std::cout << "[" << get_log_timestamp() << "][Server] Handling message from '" << session->get_session_id(true) << "': [" << raw_message.substr(0,150) << "...]" << std::endl;

    json data;
    try {
        data = json::parse(raw_message);
    } catch (json::parse_error& e) {
        std::cerr << "[" << get_log_timestamp() << "][Server] Invalid JSON from '" << session->get_session_id(true) << "': " << e.what() << ", raw: [" << raw_message << "]" << std::endl;
        session->send(common::error_event("Invalid JSON format received"));
        return;
    }

    // Проверяем наличие поля "event"
    if (!data.contains("event") || !data["event"].is_string()) {
         std::cerr << "[" << get_log_timestamp() << "][Server] Missing or invalid 'event' field from '" << session->get_session_id(true) << "': " << data.dump() << std::endl;
         session->send(common::error_event("Missing or invalid 'event' field"));
         return;
    }

    const std::string event = data["event"];

    // Маршрутизация по типу события
    if (event == "register") {
        handle_register(data, session);
    } else if (event == "login") {
        handle_login(data, session);
    } else {
        // Для всех остальных событий требуется аутентификация
        if (!session->is_authenticated()) {
             std::cerr << "[" << get_log_timestamp() << "][Server] Unauthorized event '" << event << "' from unauthenticated session " << session->get_session_id() << ". Ignoring." << std::endl;
             session->send(common::error_event("Authentication required"));
             return;
        }

        // Проверяем, совпадает ли 'from' поле (если есть) с именем пользователя сессии
        if (data.contains("from") && data["from"] != session->get_username()) {
            std::cout << "[" << get_log_timestamp() << "][Server] Warning: Mismatched 'from' field ('" << data.value("from", "") << "') in '" << event << "' message from session '" << session->get_username() << "'. Using session user." << std::endl;
        }

        // Маршрутизация аутентифицированных событий
        if (event == "message") {
            handle_message_event(data, session);
        } else if (event == "add_task") {
            handle_add_task(data, session);
        } else if (event == "list_tasks") {
            handle_list_tasks(data, session);
        }
        // else if (event == "file_offer") { handle_file_offer(data, session); }
        // ... другие обработчики ...
        else {
            std::cerr << "[" << get_log_timestamp() << "][Server] Unknown event type '" << event << "' from user '" << session->get_username() << "'." << std::endl;
            session->send(common::error_event("Unknown event type: " + event, session->get_username()));
        }
    }
}


// --- Обработчики конкретных событий ---

void TCPServer::handle_register(const json& data, std::shared_ptr<Session> session) {
     std::cout << "[" << get_log_timestamp() << "][Server] Handling 'register' event from " << session->get_session_id() << std::endl;
    if (session->is_authenticated()) {
         session->send(common::error_event("Already logged in", session->get_username()));
         return;
    }

    // Проверяем наличие полей username и password
    if (!data.contains("username") || !data["username"].is_string() ||
        !data.contains("password") || !data["password"].is_string()) {
        session->send(common::error_event("Missing 'username' or 'password' for registration"));
        return;
    }

    std::string username = data["username"];
    std::string password = data["password"];
    if (username.empty() || password.empty()) {
         session->send(common::error_event("Username and password cannot be empty"));
         return;
    }
    // TODO: Добавить валидацию сложности пароля и длины имени пользователя

    auto remote_addr = session->get_remote_address();
    bool success = db_manager_.register_user(username, password, remote_addr.first, remote_addr.second);

    if (success) {
         std::cout << "[" << get_log_timestamp() << "][Server] User '" << username << "' registered successfully." << std::endl;
         session->send(common::base_event("register_success"));
    } else {
        std::cerr << "[" << get_log_timestamp() << "][Server] Registration failed for username '" << username << "'." << std::endl;
        session->send(common::error_event("Registration failed (username might exist or DB error)"));
    }
}

void TCPServer::handle_login(const json& data, std::shared_ptr<Session> session) {
     std::cout << "[" << get_log_timestamp() << "][Server] Handling 'login' event from " << session->get_session_id() << std::endl;
     if (session->is_authenticated()) {
         session->send(common::error_event("Already logged in", session->get_username()));
         return;
     }

     if (!data.contains("username") || !data["username"].is_string() ||
         !data.contains("password") || !data["password"].is_string()) {
         session->send(common::error_event("Missing 'username' or 'password' for login"));
         return;
     }

     std::string username = data["username"];
     std::string password = data["password"];
     if (username.empty() || password.empty()) {
          session->send(common::error_event("Username and password cannot be empty"));
          return;
     }

     std::string db_ip;
     unsigned short db_port;
     bool verified = db_manager_.verify_user(username, password, db_ip, db_port);

     if (verified) {
         std::cout << "[" << get_log_timestamp() << "][Server] User '" << username << "' logged in successfully from " << session->get_session_id() << "." << std::endl;

         std::shared_ptr<Session> old_session = nullptr;
         bool first_login_since_start = false;
         { // Блок мьютекса
            std::lock_guard<std::mutex> lock(clients_mutex_);
            auto it = clients_.find(username);
            if (it != clients_.end()) { // Пользователь уже есть в карте (переподключение)
                 std::cerr << "[" << get_log_timestamp() << "][Server] User '" << username << "' reconnected. Closing previous session." << std::endl;
                 old_session = it->second; // Сохраняем старую сессию для закрытия
                 // Не инкрементируем счетчик
            } else {
                 first_login_since_start = true; // Первая сессия для этого юзера
            }
            clients_[username] = session; // Обновляем/добавляем сессию в карту
         } // Мьютекс освобождается

         if (old_session && old_session != session) {
              old_session->close_socket(); // Закрываем старую сессию
              // remove_client для старой вызовется сам
         }

         // Помечаем текущую сессию как аутентифицированную
         session->set_authenticated_user(username);

         if (first_login_since_start) {
              active_connections_gauge_->Increment(); // Увеличиваем счетчик только при первой аутентификации
         }

          // Обновляем IP/порт в БД
         auto remote_addr = session->get_remote_address();
         db_manager_.update_user_connection(username, remote_addr.first, remote_addr.second);

         // Отправляем подтверждение клиенту
         session->send(common::base_event("login_success"));

         // TODO: Возможно, стоит отправить пропущенные сообщения/уведомления

     } else {
          std::cerr << "[" << get_log_timestamp() << "][Server] Login failed for username '" << username << "' from " << session->get_session_id() << "." << std::endl;
          session->send(common::error_event("Login failed (invalid username or password)"));
          // Возможно, стоит добавить задержку или счетчик попыток
     }
}


void TCPServer::handle_message_event(const json& data, std::shared_ptr<Session> session) {
    const std::string sender = session->get_username(); // Берем из сессии, а не из JSON

    if (!data.contains("to") || !data["to"].is_string() ||
        !data.contains("text") || !data["text"].is_string()) {
        session->send(common::error_event("Message requires 'to' and 'text' fields", sender));
        return;
    }

    const std::string recipient = data["to"];
    const std::string text = data["text"];

    if (recipient.empty() || text.empty()) {
         session->send(common::error_event("Recipient ('to') and message text cannot be empty", sender));
         return;
    }

    if (recipient == sender) {
        session->send(common::error_event("Cannot send message to yourself", sender));
        return;
    }


    std::shared_ptr<Session> recipient_session = nullptr;
    { // Ищем получателя
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(recipient);
        if (it != clients_.end()) {
            recipient_session = it->second;
        }
    }

    json msg_json = common::message_event(sender, recipient, text);

    if (recipient_session) { // Если получатель онлайн
        std::cout << "[" << get_log_timestamp() << "][Server] Relaying message from '" << sender << "' to '" << recipient << "'." << std::endl;
        recipient_session->send(msg_json);
    } else { // Получатель оффлайн
        std::cout << "[" << get_log_timestamp() << "][Server] Recipient '" << recipient << "' for message from '" << sender << "' is offline." << std::endl;
        session->send(common::error_event("User '" + recipient + "' is offline.", sender));
        // TODO: Реализовать хранение оффлайн сообщений?
    }

    try { // Логируем в БД
        auto sender_addr = session->get_remote_address();
        db_manager_.log_message(sender, recipient, text, sender_addr.first);
         messages_total_counter_->Increment(); // Считаем сообщение
    } catch (const std::exception& e) {
        std::cerr << "[" << get_log_timestamp() << "][Server] DB Error logging message from '" << sender << "' to '" << recipient << "': " << e.what() << std::endl;
    }
}


void TCPServer::handle_add_task(const json& data, std::shared_ptr<Session> session) {
    const std::string username = session->get_username();

    // Проверка полей
    if (!data.contains("task_name") || !data["task_name"].is_string() ||
        !data.contains("description") || !data["description"].is_string() ||
        !data.contains("trigger_time") || !data["trigger_time"].is_string() || // Формат DD.MM.YYYY HH:MM
        !data.contains("notify_offset") || !data["notify_offset"].is_number_integer())
    {
        session->send(common::error_event("Missing or invalid fields for add_task (task_name, description, trigger_time (DD.MM.YYYY HH:MM), notify_offset (minutes))", username));
        return;
    }

    std::string task_name = data["task_name"];
    std::string description = data["description"];
    std::string user_trigger_time = data["trigger_time"];
    int notify_offset = data["notify_offset"];

    if (task_name.empty() || user_trigger_time.empty()) {
        session->send(common::error_event("Task name and trigger time cannot be empty", username));
        return;
    }
    if (notify_offset < 0) { // Допускаем 0 - уведомить точно вовремя
        session->send(common::error_event("Notify offset cannot be negative", username));
        return;
    }

    // Парсим время пользователя в ISO UTC
    // Исправлено: Предполагаем, что parse_user_time_to_iso теперь public в DatabaseManager
    auto trigger_time_iso_opt = DatabaseManager::parse_user_time_to_iso(user_trigger_time);
    if (!trigger_time_iso_opt) {
        session->send(common::error_event("Invalid trigger_time format. Use DD.MM.YYYY HH:MM", username));
        return;
    }
    std::string trigger_time_iso = *trigger_time_iso_opt;

     // TODO: Проверить, что время триггера не в прошлом?

    int task_id = db_manager_.add_task(username, task_name, description, trigger_time_iso, notify_offset);

    if (task_id != -1) {
        std::cout << "[" << get_log_timestamp() << "][Server] Task " << task_id << " added for user '" << username << "'." << std::endl;
        // Создаем объект Task для планирования
        Task new_task;
        new_task.id = task_id;
        new_task.username = username;
        new_task.task_name = task_name;
        new_task.description = description;
        new_task.trigger_time_iso = trigger_time_iso;
        new_task.notify_offset_minutes = notify_offset;
        new_task.is_active = true;
        // created_at не нужен для планирования

        schedule_task_notification(new_task); // Планируем уведомление

        json response = common::base_event("task_added");
        response["task_id"] = task_id;
        session->send(response);
    } else {
        std::cerr << "[" << get_log_timestamp() << "][Server] Failed to add task for user '" << username << "'." << std::endl;
        session->send(common::error_event("Failed to add task to database", username));
    }
}


void TCPServer::handle_list_tasks(const json& /*data*/, std::shared_ptr<Session> session) { // Убрано имя параметра data
    const std::string username = session->get_username();
     std::cout << "[" << get_log_timestamp() << "][Server] Handling 'list_tasks' for user '" << username << "'." << std::endl;

    std::vector<Task> tasks = db_manager_.get_active_tasks(username);

    json response = common::base_event("task_list");
    response["tasks"] = json::array(); // Создаем пустой массив

    for (const auto& task : tasks) {
        json task_json;
        task_json["id"] = task.id;
        task_json["name"] = task.task_name;
        task_json["description"] = task.description;
        task_json["trigger_time_iso"] = task.trigger_time_iso; // Отправляем ISO, клиент может форматировать
        task_json["notify_offset"] = task.notify_offset_minutes;
        task_json["created_at_iso"] = task.created_at_iso;
        response["tasks"].push_back(task_json);
    }

    session->send(response);
}

// --- Удаление клиента ---
void TCPServer::remove_client(const std::string& username, std::shared_ptr<Session> session_to_remove) {
     // Важно: этот метод может вызываться из обработчиков Asio,
     // поэтому доступ к clients_ должен быть синхронизирован.
     bool was_authenticated = session_to_remove->is_authenticated();
     std::string user_to_remove = username; // Имя пользователя может быть пустым, если он не успел залогиниться

     // Если имя пустое, но сессия была аутентифицирована (маловероятно, но возможно),
     // попробуем получить имя из сессии
     if (user_to_remove.empty() && was_authenticated) {
         user_to_remove = session_to_remove->get_username();
     }

     std::cout << "[" << get_log_timestamp() << "][Server] remove_client called for session_id: '" << session_to_remove->get_session_id() << "', username: '" << (user_to_remove.empty() ? "<unauthenticated>" : user_to_remove) << "'" << std::endl;

    // Закрываем сокет сессии (если еще не закрыт)
    // Делаем это до удаления из карты, чтобы предотвратить отправку сообщений
    session_to_remove->close_socket(); // close_socket теперь асинхронный через post

    if (!user_to_remove.empty()) {
        bool removed_from_map = false;
        { // Блок для мьютекса
            std::lock_guard<std::mutex> lock(clients_mutex_);
            auto it = clients_.find(user_to_remove);
            // Удаляем только если сессия в карте совпадает с той, что вызвала remove_client
            if (it != clients_.end() && it->second == session_to_remove) {
                 clients_.erase(it);
                 removed_from_map = true;
                 std::cout << "[" << get_log_timestamp() << "][Server DEBUG] Removed user '" << user_to_remove << "' from active clients map." << std::endl;
            } else if (it != clients_.end()){
                 std::cout << "[" << get_log_timestamp() << "][Server DEBUG] remove_client: Session for user '" << user_to_remove << "' in map differs. Not removing map entry (already replaced?)." << std::endl;
            } else {
                 std::cout << "[" << get_log_timestamp() << "][Server DEBUG] remove_client: User '" << user_to_remove << "' not found in map (already removed?)." << std::endl;
            }
        } // Мьютекс освобождается

        // Уменьшаем счетчик только если сессия была аутентифицирована И была удалена из карты
        // (чтобы не уменьшить дважды, если remove_client вызвался несколько раз для одной сессии)
        if (was_authenticated && removed_from_map) {
            active_connections_gauge_->Decrement();
            std::cout << "[" << get_log_timestamp() << "][Server] User '" << user_to_remove << "' disconnected. Active connections: " << active_connections_gauge_->Value() << std::endl;
        }
    } else {
        // Сессия закрылась до аутентификации
        std::cout << "[" << get_log_timestamp() << "][Server] Unauthenticated connection closed." << std::endl;
        // Счетчик не трогаем
    }
     // shared_ptr на сессию будет уничтожен, когда последний указатель на него исчезнет
     // (например, из обработчика Asio и из clients_, если он там был)
}


// --- Task Scheduling Implementation ---

// Парсит ISO 8601 строку в time_point
std::optional<std::chrono::system_clock::time_point> parse_iso_time(const std::string& iso_time_str) {
    std::tm tm = {};
    std::stringstream ss(iso_time_str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    if (ss.fail()) {
        return std::nullopt;
    }
    // std::mktime считает локальным, timegm (если есть) считает UTC.
    // Т.к. мы сохраняли UTC, нам нужен time_t соответствующий UTC.
    // time_t time = timegm(&tm); // Идеальный вариант
    // Временное решение (может быть неточно из-за локального пояса mktime):
    tm.tm_isdst = 0;
    time_t time = std::mktime(&tm);
    if (time == -1) return std::nullopt;

    // TODO: Корректно учесть, что mktime возвращает локальный time_t,
    // а нам нужен system_clock::time_point, соответствующий UTC моменту tm.
    // Пока предполагаем, что system_clock эпоха совпадает с UTC эпохой.
    return std::chrono::system_clock::from_time_t(time);
}


void TCPServer::schedule_task_notification(const Task& task) {
    std::cout << "[" << get_log_timestamp() << "][Scheduler] Scheduling task ID: " << task.id << " for user '" << task.username << "' at " << task.trigger_time_iso << " (offset: " << task.notify_offset_minutes << " min)" << std::endl;

    auto trigger_time_point_opt = parse_iso_time(task.trigger_time_iso);
    if (!trigger_time_point_opt) {
         std::cerr << "[" << get_log_timestamp() << "][Scheduler] Failed to parse trigger time for task " << task.id << ": " << task.trigger_time_iso << std::endl;
         return;
    }
    auto trigger_time_point = *trigger_time_point_opt;

    // Вычитаем смещение уведомления
    auto notify_time_point = trigger_time_point - std::chrono::minutes(task.notify_offset_minutes);
    // Рассчитываем время до уведомления от текущего момента
    auto system_now = std::chrono::system_clock::now();
    if (notify_time_point <= system_now) {
         std::cout << "[" << get_log_timestamp() << "][Scheduler] Task " << task.id << " notification time is in the past. Skipping." << std::endl;
         // Можно либо уведомить немедленно, либо просто пометить как неактивную
         db_manager_.mark_task_inactive(task.id);
         return;
    }

     // Используем steady_timer, которому нужно время *относительно* now()
    auto duration_until_notify = std::chrono::duration_cast<std::chrono::steady_clock::duration>(notify_time_point - system_now);

    // Создаем таймер
    auto timer = std::make_shared<boost::asio::steady_timer>(io_context_);

    // Сохраняем таймер в карту (под мьютексом)
    {
        std::lock_guard<std::mutex> lock(task_timers_mutex_);
         // Отменяем и удаляем старый таймер, если он был для этой задачи
         auto it = task_timers_.find(task.id);
         if (it != task_timers_.end()) {
             it->second->cancel(); // Старый cancel() не принимает ec
             task_timers_.erase(it);
             std::cout << "[" << get_log_timestamp() << "][Scheduler] Replaced existing timer for task " << task.id << std::endl;
         }
        task_timers_[task.id] = timer;
    }

    // Устанавливаем время срабатывания
    timer->expires_after(duration_until_notify);

     std::cout << "[" << get_log_timestamp() << "][Scheduler] Timer set for task " << task.id << " in "
               << std::chrono::duration_cast<std::chrono::seconds>(duration_until_notify).count() << " seconds." << std::endl;


    // Запускаем таймер с обработчиком
    // Используем strand или post, чтобы обработчик выполнялся в io_context потоке
    // Используем копирование task по значению в лямбду
    timer->async_wait(boost::asio::bind_executor(io_context_.get_executor(), // Гарантируем выполнение в io_context
        [this, task_copy = task, task_id = task.id](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted) {
                 std::cout << "[" << get_log_timestamp() << "][Scheduler] Timer for task " << task_id << " cancelled." << std::endl;
                 // Удаляем из карты при отмене (возможно, уже удален при stop())
                  std::lock_guard<std::mutex> lock(task_timers_mutex_);
                  task_timers_.erase(task_id);
                 return;
            } else if (ec) {
                 std::cerr << "[" << get_log_timestamp() << "][Scheduler] Timer error for task " << task_id << ": " << ec.message() << std::endl;
                 // Удаляем из карты при ошибке
                  std::lock_guard<std::mutex> lock(task_timers_mutex_);
                  task_timers_.erase(task_id);
                 return;
            }

             // Время пришло!
             std::cout << "[" << get_log_timestamp() << "][Scheduler] Task " << task_id << " triggered for user '" << task_copy.username << "'!" << std::endl;

             // Находим сессию пользователя
             std::shared_ptr<Session> user_session = nullptr;
             {
                 std::lock_guard<std::mutex> lock(clients_mutex_);
                 auto it = clients_.find(task_copy.username);
                 if (it != clients_.end()) {
                     user_session = it->second;
                 }
             }

             // Отправляем уведомление, если пользователь онлайн
             if (user_session) {
                 std::cout << "[" << get_log_timestamp() << "][Scheduler] Sending notification for task " << task_id << " to online user '" << task_copy.username << "'." << std::endl;
                 user_session->send(common::task_notification_event(task_copy.username, task_id, task_copy.task_name, task_copy.description));
             } else {
                  std::cout << "[" << get_log_timestamp() << "][Scheduler] User '" << task_copy.username << "' for task " << task_id << " is offline. Notification skipped." << std::endl;
                  // TODO: Сохранять пропущенные уведомления?
             }

             // Помечаем задачу как неактивную в БД
             db_manager_.mark_task_inactive(task_id);

             // Удаляем таймер из карты
             {
                  std::lock_guard<std::mutex> lock(task_timers_mutex_);
                  task_timers_.erase(task_id);
                  // std::cout << "[Scheduler DEBUG] Removed timer for task " << task_id << " from map." << std::endl;
             }
        }
    ));
}

void TCPServer::cancel_task_notification(int task_id) {
     std::lock_guard<std::mutex> lock(task_timers_mutex_);
     auto it = task_timers_.find(task_id);
     if (it != task_timers_.end()) {
          // Исправлено: cancel() не принимает error_code
          std::size_t cancelled_count = it->second->cancel();
          if (cancelled_count > 0) {
               std::cout << "[" << get_log_timestamp() << "][Scheduler] Cancelled timer for task " << task_id << "." << std::endl;
          }
          task_timers_.erase(it); // Удаляем из карты в любом случае
     }
}


void TCPServer::load_and_schedule_tasks() {
     std::cout << "[" << get_log_timestamp() << "][Scheduler] Loading active tasks from database..." << std::endl;
     std::vector<Task> active_tasks = db_manager_.get_all_active_tasks();
     std::cout << "[" << get_log_timestamp() << "][Scheduler] Found " << active_tasks.size() << " active tasks." << std::endl;

     int scheduled_count = 0;
     for (const auto& task : active_tasks) {
          try {
            schedule_task_notification(task);
            scheduled_count++;
          } catch (const std::exception& e) {
              std::cerr << "[" << get_log_timestamp() << "][Scheduler] Error scheduling task ID " << task.id << ": " << e.what() << std::endl;
               // Решаем, что делать - пометить задачу как неактивную?
               // db_manager_.mark_task_inactive(task.id);
          }
     }
      std::cout << "[" << get_log_timestamp() << "][Scheduler] Successfully scheduled " << scheduled_count << " tasks." << std::endl;
}


void TCPServer::start_cleanup_timer() {
     std::cout << "[" << get_log_timestamp() << "][Server] Starting periodic DB cleanup timer (interval: " << DEFAULT_CLEANUP_INTERVAL_HOURS << " hours)." << std::endl;
     cleanup_timer_.expires_after(std::chrono::hours(DEFAULT_CLEANUP_INTERVAL_HOURS));
     cleanup_timer_.async_wait(boost::asio::bind_executor(io_context_.get_executor(),
          std::bind(&TCPServer::perform_db_cleanup, this, std::placeholders::_1))
     );
}

void TCPServer::perform_db_cleanup(const boost::system::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) {
         std::cout << "[" << get_log_timestamp() << "][Server] DB cleanup timer cancelled." << std::endl;
         return;
    } else if (ec) {
         std::cerr << "[" << get_log_timestamp() << "][Server] DB cleanup timer error: " << ec.message() << std::endl;
         // Попробовать перезапустить таймер?
    } else {
        std::cout << "[" << get_log_timestamp() << "][Server] Performing scheduled DB cleanup..." << std::endl;
        try {
            int count = db_manager_.cleanup_inactive_tasks(DEFAULT_CLEANUP_DAYS);
             std::cout << "[" << get_log_timestamp() << "][Server] DB cleanup finished. Removed " << count << " tasks." << std::endl;
        } catch (const std::exception& e) {
             std::cerr << "[" << get_log_timestamp() << "][Server] Exception during DB cleanup: " << e.what() << std::endl;
        }
    }

     // Перезапускаем таймер для следующего цикла, если сервер не останавливается
     if (acceptor_.is_open()) { // Проверяем, работает ли еще сервер
         cleanup_timer_.expires_after(std::chrono::hours(DEFAULT_CLEANUP_INTERVAL_HOURS));
         cleanup_timer_.async_wait(boost::asio::bind_executor(io_context_.get_executor(),
              std::bind(&TCPServer::perform_db_cleanup, this, std::placeholders::_1))
         );
     } else {
          std::cout << "[" << get_log_timestamp() << "][Server] Server stopped, not rescheduling cleanup timer." << std::endl;
     }
}


} // namespace tcp_messenger
