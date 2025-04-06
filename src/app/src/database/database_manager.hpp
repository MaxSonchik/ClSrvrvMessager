#ifndef DATABASE_MANAGER_HPP
#define DATABASE_MANAGER_HPP

#include <string>
#include <sqlite3.h>
#include <stdexcept>
#include <mutex>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace tcp_messenger {

class DatabaseManager {
public:
    DatabaseManager(const std::string& db_path);
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    void register_user(const std::string& username, const std::string& ip_address, unsigned short port);
    void log_message(const std::string& sender_username,
                     const std::string& recipient_username,
                     const std::string& message_text,
                     const std::string& sender_ip_address);

private:
    void initialize_database();
    static std::string get_current_timestamp();

    sqlite3* db_ = nullptr;
    std::string db_path_;
    std::mutex db_mutex_;
};

} 

#endif 