#include "tcp_server.hpp"
#include "common/json_util.h" // Утилита для создания JSON
#include <iostream>
#include <istream>
#include <memory>
#include <nlohmann/json.hpp> // JSON библиотека
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp> // Можно убрать, если используем только лямбды

// Псевдонимы
using json = nlohmann::json;
using boost::asio::ip::tcp;

namespace tcp_messenger {

// --- Определение класса TCPServer::Session ---
class TCPServer::Session : public std::enable_shared_from_this<TCPServer::Session> {
public:
    // Конструктор
    Session(TCPServer& server, tcp::socket socket)
        : server_(server), socket_(std::move(socket)), remote_ep_str_("unknown") {
        try {
            remote_ep_str_ = socket_.remote_endpoint().address().to_string() + ":" + std::to_string(socket_.remote_endpoint().port());
        } catch (const std::exception& e) {
            std::cerr << "[" << get_log_timestamp() << "][Session DEBUG] Error getting remote endpoint on creation: " << e.what() << std::endl;
        }
        std::cout << "[" << get_log_timestamp() << "][Session] Created for endpoint: " << remote_ep_str_ << std::endl;
    }

    // Деструктор
    ~Session() {
         std::cout << "[" << get_log_timestamp() << "][Session] Destroyed for user: '" << get_session_id() << "'" << std::endl;
    }

    // Начать цикл чтения
    void start() {
        std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Starting read loop." << std::endl;
        do_read();
    }

    // Отправить данные асинхронно
    void send(const std::string& data) {
        if (!socket_.is_open()) {
             std::cerr << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Attempted to send data on closed socket. Data: [" << data.substr(0,100) << "...]" << std::endl;
             return;
        }
        std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Sending data: [" << data.substr(0, 100) << "...]" << std::endl;
        auto self = shared_from_this();
        boost::asio::async_write(socket_, boost::asio::buffer(data),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Sent " << length << " bytes successfully." << std::endl;
                } else {
                     std::cerr << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Write error: " << ec.message() << " (" << ec.value() << ")" << std::endl;
                     // Не вызываем remove_client здесь, ошибка чтения обработает отключение
                     // Попробуем закрыть сокет, чтобы прервать чтение
                     close_socket();
                }
            });
    }

    // Геттеры
    std::string get_username() const { return username_; }
    std::pair<std::string, unsigned short> get_remote_address() const {
        boost::system::error_code ec;
        tcp::endpoint endpoint = socket_.remote_endpoint(ec);
        if (ec) { return {"unknown", 0}; }
        return {endpoint.address().to_string(), endpoint.port()};
    }

    // Сеттер для имени пользователя
    void set_username(const std::string& name) {
        std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id(true) << "] Setting username to: '" << name << "'" << std::endl;
        username_ = name;
    }

    // Публичный метод для закрытия сокета (вызывается из TCPServer::stop или при ошибках)
    void close_socket() {
        if (socket_.is_open()) {
            std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Closing socket." << std::endl;
            boost::system::error_code ec_shutdown, ec_close;
            socket_.shutdown(tcp::socket::shutdown_both, ec_shutdown);
            if (ec_shutdown && ec_shutdown != boost::asio::error::not_connected && ec_shutdown != boost::asio::error::connection_reset) {
                 std::cerr << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Error shutting down socket: " << ec_shutdown.message() << std::endl;
            }
            socket_.close(ec_close);
             if (ec_close) {
                 std::cerr << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Error closing socket: " << ec_close.message() << std::endl;
            }
        } else {
             // std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Socket already closed." << std::endl;
        }
    }

    // Получить идентификатор сессии для логов (имя пользователя или удаленный адрес)
    std::string get_session_id(bool prefer_username = false) const {
         if (prefer_username && !username_.empty()) return username_;
         if (!username_.empty()) return username_;
         return remote_ep_str_;
    }

private:
    // Асинхронное чтение данных
    void do_read() {
        // std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] do_read() called." << std::endl;
        auto self = shared_from_this();
        boost::asio::async_read_until(socket_, buffer_, '\n',
            [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
                // std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] async_read_until completed. ErrorCode: " << ec.message() << " (" << ec.value() << ")" << ", Bytes: " << bytes_transferred << std::endl;

                if (!ec) { // Успешное чтение
                    std::istream is(&buffer_);
                    std::string line;
                    std::getline(is, line);
                    // std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Read line: [" << line << "]" << std::endl;

                    if (!line.empty()) {
                        server_.handle_message(line, self);
                    } else {
                         // Игнорируем пустые строки
                         // std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Read empty line." << std::endl;
                    }
                    if(socket_.is_open()) { // Продолжаем читать, если сокет открыт
                         do_read();
                    } else {
                         std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Socket closed after read, stopping read loop." << std::endl;
                         // Соединение закрылось между чтением и проверкой, удаляем клиента
                         server_.remove_client(username_, self);
                    }
                } else if (ec == boost::asio::error::eof) { // Клиент закрыл соединение
                    std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Connection closed by peer (EOF)." << std::endl;
                    server_.remove_client(username_, self);
                } else if (ec == boost::asio::error::operation_aborted) { // Операция отменена (сокет закрыт извне)
                     std::cout << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Read operation aborted." << std::endl;
                     // Не вызываем remove_client здесь, он уже должен быть вызван
                } else { // Другая ошибка чтения
                     std::cerr << "[" << get_log_timestamp() << "][Session:" << get_session_id() << "] Read error: " << ec.message() << " (" << ec.value() << ")" << std::endl;
                     server_.remove_client(username_, self);
                }
            });
    }

    // Приватные члены
    TCPServer& server_;
    tcp::socket socket_;
    boost::asio::streambuf buffer_;
    std::string username_;
    std::string remote_ep_str_; // Строковое представление удаленной точки для логов
}; // --- Конец определения класса Session ---


// --- Реализация методов TCPServer ---

// Конструктор
TCPServer::TCPServer(boost::asio::io_context& io_context,
                     const std::string& db_path,
                     unsigned short app_port,
                     unsigned short metrics_port)
    : io_context_(io_context),
      acceptor_(io_context, tcp::endpoint(tcp::v4(), app_port)),
      db_manager_(db_path), // Может выбросить исключение, если БД не откроется
      exposer_("0.0.0.0:" + std::to_string(metrics_port)),
      registry_(std::make_shared<prometheus::Registry>())
{
    try {
        messages_total_counter_ = &prometheus::BuildCounter()
              .Name("messenger_messages_total")
              .Help("Total processed messages by the messenger server")
              .Register(*registry_)
              .Add({});

        active_connections_gauge_ = &prometheus::BuildGauge()
              .Name("messenger_active_connections")
              .Help("Current active TCP connections to the messenger server")
              .Register(*registry_)
              .Add({});

        exposer_.RegisterCollectable(registry_);
        active_connections_gauge_->Set(0);

        std::cout << "[" << get_log_timestamp() << "][Server] Listening on TCP port " << app_port << std::endl;
        std::cout << "[" << get_log_timestamp() << "][Server] Exposing metrics on port " << metrics_port << std::endl;
        std::cout << "[" << get_log_timestamp() << "][Server] Database path: " << db_path << std::endl;
    } catch (const std::exception& e) {
         std::cerr << "[" << get_log_timestamp() << "][Server] CRITICAL ERROR during Prometheus setup: " << e.what() << std::endl;
         // Перебрасываем исключение, чтобы приложение не запустилось некорректно
         throw;
    }
}

// Запуск сервера
void TCPServer::start() {
    std::cout << "[" << get_log_timestamp() << "][Server] Starting acceptor..." << std::endl;
    do_accept(); // Начинаем цикл приема соединений
}

// Цикл приема соединений
void TCPServer::do_accept() {
    // std::cout << "[" << get_log_timestamp() << "][Server DEBUG] do_accept() called." << std::endl;
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            // std::cout << "[" << get_log_timestamp() << "][Server DEBUG] async_accept completed. ErrorCode: " << ec.message() << std::endl;
            if (!ec) {
                // Успешно приняли соединение
                std::cout << "[" << get_log_timestamp() << "][Server] Accepted connection from "
                          << socket.remote_endpoint().address().to_string() << ":"
                          << socket.remote_endpoint().port() << std::endl;
                try {
                    // Создаем сессию для нового клиента
                    auto new_session = std::make_shared<Session>(*this, std::move(socket));
                    new_session->start(); // Запускаем чтение
                } catch (const std::bad_alloc& ba) {
                    std::cerr << "[" << get_log_timestamp() << "][Server] CRITICAL ERROR: Failed to allocate memory for new session!" << std::endl;
                    // Пытаемся закрыть принятый сокет, чтобы не оставить его висеть
                    boost::system::error_code close_ec;
                    socket.close(close_ec);
                } catch (const std::exception& e) {
                     std::cerr << "[" << get_log_timestamp() << "][Server] Error creating session: " << e.what() << std::endl;
                     boost::system::error_code close_ec;
                     socket.close(close_ec);
                }
            } else {
                // Ошибка при приеме соединения
                std::cerr << "[" << get_log_timestamp() << "][Server] Accept error: " << ec.message() << std::endl;
                if (ec == boost::asio::error::operation_aborted) {
                     std::cout << "[" << get_log_timestamp() << "][Server] Accept operation aborted (likely server stopping)." << std::endl;
                     return; // Не продолжаем цикл, если acceptor закрыт
                }
                // Можно добавить обработку других ошибок, если нужно (например, too many open files)
            }

            // Продолжаем слушать новые соединения, если сервер не остановлен
            if (acceptor_.is_open()) {
                 do_accept();
            } else {
                 std::cout << "[" << get_log_timestamp() << "][Server] Acceptor closed, stopping accept loop." << std::endl;
            }
        });
}

// Обработка сообщения от клиента
void TCPServer::handle_message(const std::string& message, std::shared_ptr<Session> session) {
    std::cout << "[" << get_log_timestamp() << "][Server] Handling message from '" << session->get_session_id(true) << "': [" << message.substr(0,150) << "...]" << std::endl;

    json data;
    try {
        data = json::parse(message); // Парсим JSON
    } catch (json::parse_error& e) {
        std::cerr << "[" << get_log_timestamp() << "][Server] Invalid JSON from '" << session->get_session_id(true) << "': " << message << ", error: " << e.what() << std::endl;
        session->send(common::make_json_event("error", "server", session->get_username(), "Invalid JSON format") + "\n");
        return;
    }

    const std::string event = data.value("event", "");
    std::string user_from_msg = data.value("from", ""); // Может быть пустым

    // --- Обработка 'connect' ---
    if (event == "connect") {
        if (user_from_msg.empty()) { // Имя пользователя обязательно для connect
             std::cerr << "[" << get_log_timestamp() << "][Server] Connect event received without 'from' username from " << session->get_session_id() << "." << std::endl;
             session->send(common::make_json_event("error", "server", "", "Username ('from' field) is required for 'connect' event.") + "\n");
             // Решаем закрыть такое соединение, т.к. оно бесполезно без идентификации
             session->close_socket();
             // remove_client будет вызван автоматически из-за ошибки/закрытия сокета
             return;
        }

        // Проверяем, не пытается ли уже идентифицированная сессия сменить имя
        if (!session->get_username().empty() && session->get_username() != user_from_msg) {
             std::cerr << "[" << get_log_timestamp() << "][Server] Session already identified as '" << session->get_username() << "' received connect for different user '" << user_from_msg << "'. Ignoring." << std::endl;
             session->send(common::make_json_event("error", "server", session->get_username(), "Already connected as a different user.") + "\n");
             return;
        }

        // Устанавливаем имя пользователя для сессии
        session->set_username(user_from_msg);
        auto remote_addr = session->get_remote_address();
        bool is_new_connection = false;
        std::shared_ptr<Session> old_session = nullptr;

        { // Блок для мьютекса
            std::lock_guard<std::mutex> lock(clients_mutex_);
            auto it = clients_.find(user_from_msg);
            if (it == clients_.end()) { // Пользователя еще нет в карте
                 is_new_connection = true;
            } else { // Пользователь уже есть - переподключение
                 std::cerr << "[" << get_log_timestamp() << "][Server] User '" << user_from_msg << "' reconnected. Closing previous session." << std::endl;
                 old_session = it->second; // Сохраняем указатель на старую сессию, чтобы закрыть вне мьютекса
                 // is_new_connection остается false
            }
            clients_[user_from_msg] = session; // Добавляем/обновляем сессию
        } // Мьютекс освобождается

        if (old_session && old_session != session) { // Если была старая сессия и она не эта же самая
             old_session->close_socket(); // Закрываем старую сессию
             // remove_client для старой сессии вызовется автоматически при ошибке/закрытии сокета
        }

        if (is_new_connection) { // Увеличиваем счетчик только для новых имен
             active_connections_gauge_->Increment();
        }

        try { // Обновляем данные в БД
            db_manager_.register_user(user_from_msg, remote_addr.first, remote_addr.second);
        } catch (const std::exception& e) {
            std::cerr << "[" << get_log_timestamp() << "][Server] DB Error registering user '" << user_from_msg << "': " << e.what() << std::endl;
        }
        std::cout << "[" << get_log_timestamp() << "][Server] User '" << user_from_msg << "' connected. Total active connections: " << active_connections_gauge_->Value() << std::endl;
        session->send(common::make_json_event("status", "server", user_from_msg, "Connected successfully") + "\n");
        return; // Конец обработки 'connect'
    }

    // --- Обработка других событий (требуется идентифицированная сессия) ---
    const std::string session_user = session->get_username();
    if (session_user.empty()) {
         std::cerr << "[" << get_log_timestamp() << "][Server] Received '" << event << "' event from unidentified session " << session->get_session_id() << ". Ignoring." << std::endl;
         session->send(common::make_json_event("error", "server", "", "Please 'connect' first with your username.") + "\n");
         return;
    }
    // Проверяем поле 'from' в сообщении (не обязательно, но полезно для отладки)
    if (!user_from_msg.empty() && user_from_msg != session_user) {
         std::cout << "[" << get_log_timestamp() << "][Server] Warning: Mismatched 'from' field ('" << user_from_msg << "') in message from session '" << session_user << "'. Using session user." << std::endl;
    }

    // --- Обработка 'message' ---
    if (event == "message") {
        const std::string to_user = data.value("to", "");
        const std::string text = data.value("text", "");

        if (to_user.empty() || text.empty()) { // Проверка наличия полей
             std::cerr << "[" << get_log_timestamp() << "][Server] Message event from '" << session_user << "' is missing 'to' or 'text' field." << std::endl;
             session->send(common::make_json_event("error", "server", session_user, "Message requires non-empty 'to' and 'text' fields.") + "\n");
             return;
        }

        std::shared_ptr<Session> recipient_session = nullptr;
        { // Ищем получателя
            std::lock_guard<std::mutex> lock(clients_mutex_);
            auto it = clients_.find(to_user);
            if (it != clients_.end()) {
                recipient_session = it->second;
            }
        }

        if (recipient_session) { // Отправляем, если онлайн
            std::cout << "[" << get_log_timestamp() << "][Server] Relaying message from '" << session_user << "' to '" << to_user << "'." << std::endl;
            recipient_session->send(common::make_json_event("message", session_user, to_user, text) + "\n");
        } else { // Сообщаем об ошибке, если оффлайн
            std::cout << "[" << get_log_timestamp() << "][Server] Recipient '" << to_user << "' for message from '" << session_user << "' is offline." << std::endl;
             session->send(common::make_json_event("error", "server", session_user, "User '" + to_user + "' is offline or does not exist.") + "\n");
        }

        try { // Логируем в БД
             auto sender_addr = session->get_remote_address();
             db_manager_.log_message(session_user, to_user, text, sender_addr.first);
        } catch (const std::exception& e) {
             std::cerr << "[" << get_log_timestamp() << "][Server] DB Error logging message from '" << session_user << "' to '" << to_user << "': " << e.what() << std::endl;
        }
         messages_total_counter_->Increment(); // Считаем только 'message' события

    }
    // --- Обработка неизвестных событий ---
    else {
         std::cerr << "[" << get_log_timestamp() << "][Server] Unknown event type '" << event << "' received from user '" << session_user << "'." << std::endl;
         session->send(common::make_json_event("error", "server", session_user, "Unknown event type received: " + event) + "\n");
    }
} // --- Конец handle_message ---


// --- Удаление клиента ---
// Вызывается из Session при ошибке сокета или EOF
void TCPServer::remove_client(const std::string& username, std::shared_ptr<Session> session_to_remove) {
     std::cout << "[" << get_log_timestamp() << "][Server] remove_client called for username: '" << (username.empty() ? "<empty>" : username) << "'" << std::endl;

    if (!username.empty()) { // Если имя было установлено
        bool removed_from_map = false;
        { // Блок для мьютекса
            std::lock_guard<std::mutex> lock(clients_mutex_);
            auto it = clients_.find(username);
            // Удаляем только если сессия в карте совпадает с той, что вызвала remove_client
            // Это предотвращает удаление новой сессии из-за ошибки старой
            if (it != clients_.end() && it->second == session_to_remove) {
                 clients_.erase(it);
                 removed_from_map = true;
            } else if (it != clients_.end()){
                 // Сессия с таким именем есть, но это не та, что вызвала удаление (значит, ее уже заменили)
                 std::cout << "[" << get_log_timestamp() << "][Server DEBUG] remove_client: Session for user '" << username << "' was already replaced. Not removing from map again." << std::endl;
            } else {
                 // Сессии с таким именем уже нет в карте
                  std::cout << "[" << get_log_timestamp() << "][Server DEBUG] remove_client: User '" << username << "' not found in map." << std::endl;
            }
        } // Мьютекс освобождается

        if (removed_from_map) { // Если удалили из карты
            active_connections_gauge_->Decrement(); // Уменьшаем счетчик
            std::cout << "[" << get_log_timestamp() << "][Server] User '" << username << "' disconnected. Total active connections: " << active_connections_gauge_->Value() << std::endl;
        }
    } else {
        // Сессия закрылась до отправки 'connect'
        std::cout << "[" << get_log_timestamp() << "][Server] Unidentified connection closed." << std::endl;
        // Счетчик не трогаем
    }
    // Явно закрывать сокет здесь не нужно, он уже должен быть закрыт (или закроется сам)
    // session_to_remove->close_socket(); // Это может привести к двойному закрытию
} // --- Конец remove_client ---

// --- Остановка сервера ---
void TCPServer::stop() {
    std::cout << "[" << get_log_timestamp() << "][Server] Gracefully stopping server..." << std::endl;
    boost::system::error_code ec;

    // 1. Прекращаем принимать новые соединения
    acceptor_.close(ec);
    if (ec) { std::cerr << "[" << get_log_timestamp() << "][Server] Error closing acceptor: " << ec.message() << std::endl; }
    else { std::cout << "[" << get_log_timestamp() << "][Server] Acceptor closed." << std::endl; }

    // 2. Закрываем все активные клиентские сессии
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        if (clients_.empty()) {
             std::cout << "[" << get_log_timestamp() << "][Server] No active client connections to close." << std::endl;
        } else {
            std::cout << "[" << get_log_timestamp() << "][Server] Closing " << clients_.size() << " active client connection(s)..." << std::endl;
            for (auto const& [name, session_ptr] : clients_) {
                 session_ptr->close_socket(); // Вызываем метод сессии для закрытия сокета
            }
            clients_.clear(); // Очищаем карту
        }
    } // Мьютекс освобождается

    // 3. Сбрасываем счетчик
    active_connections_gauge_->Set(0);
    std::cout << "[" << get_log_timestamp() << "][Server] Stop sequence finished." << std::endl;
} // --- Конец stop ---

} // namespace tcp_messenger