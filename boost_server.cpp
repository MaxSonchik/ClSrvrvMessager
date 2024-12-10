#include <boost/asio.hpp>
#include <sqlite3.h>
#include <iostream>
#include <map>
#include <memory>
#include <thread>
#include <mutex>

using boost::asio::ip::tcp;

class Server {
public:
    Server(boost::asio::io_context& io_context, short tcp_port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), tcp_port)) {
        initialize_database();
        start_accept();
    }

private:
    void start_accept() {
        auto new_socket = std::make_shared<tcp::socket>(acceptor_.get_executor());
        acceptor_.async_accept(*new_socket,
            [this, new_socket](const boost::system::error_code& error) {
                if (!error) {
                    handle_client(new_socket);
                }
                start_accept();
            });
    }

    void handle_client(std::shared_ptr<tcp::socket> socket) {
        std::thread([this, socket]() {
            int client_id = -1;

            try {
                char data[1024];

                // Получение ID клиента
                size_t length = socket->read_some(boost::asio::buffer(data));
                client_id = std::stoi(std::string(data, length));

                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    clients_[client_id] = socket;
                }

                std::cout << "Client " << client_id << " connected." << std::endl;

                int active_chat_id = -1;

                while (true) {
                    length = socket->read_some(boost::asio::buffer(data));
                    std::string message(data, length);

                    if (message.find("START:") == 0) {
                        active_chat_id = std::stoi(message.substr(6));
                        std::cout << "Client " << client_id << " started chat with " << active_chat_id << std::endl;
                    } else if (message == "EXIT") {
                        active_chat_id = -1;
                        std::cout << "Client " << client_id << " exited chat." << std::endl;
                    } else if (active_chat_id != -1) {
                        forward_message(client_id, active_chat_id, message);
                    }
                }
            } catch (std::exception& e) {
                std::cerr << "Client disconnected: " << e.what() << std::endl;
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = clients_.find(client_id);
                if (it != clients_.end()) {
                    clients_.erase(it);
                }
            }
        }).detach();
    }

    void forward_message(int sender_id, int receiver_id, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = clients_.find(receiver_id);
        if (it != clients_.end()) {
            auto socket = it->second;
            std::string full_message = std::to_string(sender_id) + ": " + message;
            boost::asio::write(*socket, boost::asio::buffer(full_message));

            // Логирование сообщения
            log_message(sender_id, receiver_id, message);
        } else {
            std::cerr << "Receiver " << receiver_id << " not found." << std::endl;
        }
    }

    void initialize_database() {
        if (sqlite3_open("logs.db", &db_) != SQLITE_OK) {
            throw std::runtime_error("Failed to open database");
        }

        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                sender_id INTEGER NOT NULL,
                receiver_id INTEGER NOT NULL,
                message TEXT NOT NULL
            );
        )";

        char* error_msg = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
            std::string error = error_msg;
            sqlite3_free(error_msg);
            throw std::runtime_error("Failed to create table: " + error);
        }
    }

    void log_message(int sender_id, int receiver_id, const std::string& message) {
        const char* sql = R"(
            INSERT INTO messages (sender_id, receiver_id, message)
            VALUES (?, ?, ?);
        )";

        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare statement");
        }

        sqlite3_bind_int(stmt, 1, sender_id);
        sqlite3_bind_int(stmt, 2, receiver_id);
        sqlite3_bind_text(stmt, 3, message.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            throw std::runtime_error("Failed to execute statement");
        }

        sqlite3_finalize(stmt);
    }

    tcp::acceptor acceptor_;
    std::map<int, std::shared_ptr<tcp::socket>> clients_;
    std::mutex mutex_;
    sqlite3* db_;
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: server <port>" << std::endl;
            return 1;
        }

        boost::asio::io_context io_context;
        Server server(io_context, std::atoi(argv[1]));
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
