#include <boost/asio.hpp>
#include <sqlite3.h>
#include <iostream>
#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>

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
            static std::atomic<int> next_client_id{1};
            int client_id = next_client_id.fetch_add(1);

            try {
                char data[1024];
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    clients_[client_id] = socket;
                }

                // Выводим в консоль информацию о подключении клиента
                std::cout << "Client connected. Assigned ID: " << client_id 
                          << ", IP: " << socket->remote_endpoint().address().to_string() 
                          << std::endl;

                while (true) {
                    size_t length = socket->read_some(boost::asio::buffer(data));
                    std::string message(data, length);

                    if (message == "EXIT") {
                        std::cout << "Client " << client_id << " exited chat." << std::endl;
                        break;
                    } else {
                        std::cout << "Message from client " << client_id << ": " << message << std::endl;
                    }
                }
            } catch (std::exception& e) {
                std::cerr << "Client disconnected: " << e.what() << std::endl;
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                clients_.erase(client_id);
            }
        }).detach();
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

    tcp::acceptor acceptor_;
    std::map<int, std::shared_ptr<tcp::socket>> clients_;
    std::mutex mutex_;
    sqlite3* db_;
};

int main() {
    try {
        const short port = 8080; // Замените на нужный порт

        boost::asio::io_context io_context;
        Server server(io_context, port);
        std::cout << "Server is running on port " << port << "..." << std::endl;
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
