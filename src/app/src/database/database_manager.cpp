#include "database_manager.hpp"
#include <stdexcept> 
#include <iostream>  

namespace tcp_messenger {

std::string DatabaseManager::get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}


DatabaseManager::DatabaseManager(const std::string& db_path) : db_path_(db_path) {
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    int rc = sqlite3_open_v2(db_path_.c_str(), &db_, flags, nullptr);

    if (rc != SQLITE_OK) {
        std::string errmsg = db_ ? sqlite3_errmsg(db_) : "SQLite failed to allocate memory for error message";
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Cannot open/create database '" + db_path + "': " + errmsg);
    }
    std::cout << "[Database] Opened successfully: " << db_path_ << std::endl;

    try {
        initialize_database();
    } catch (...) {
        sqlite3_close(db_);
        db_ = nullptr;
        throw;
    }
}

DatabaseManager::~DatabaseManager() {
    if (db_) {
        int rc = sqlite3_close(db_);
        if (rc != SQLITE_OK) {
             std::cerr << "[Database] Error closing database: " << sqlite3_errmsg(db_) << std::endl;
        } else {
             std::cout << "[Database] Closed: " << db_path_ << std::endl;
        }
    }
}

void DatabaseManager::initialize_database() {
    const char* create_users_table_sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "username TEXT PRIMARY KEY NOT NULL,"
        "ip_address TEXT,"
        "port INTEGER,"
        "last_connected_at TEXT"
        ");";

    const char* create_messages_table_sql =
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "sender_username TEXT NOT NULL,"
        "recipient_username TEXT NOT NULL,"
        "message_text TEXT,"
        "timestamp TEXT NOT NULL,"
        "sender_ip_address TEXT"
        ");";

    char* err_msg = nullptr;
    std::lock_guard<std::mutex> lock(db_mutex_);

    int rc = sqlite3_exec(db_, create_users_table_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error_message = "[Database] SQL error creating/checking 'users' table: " + std::string(err_msg);
        sqlite3_free(err_msg);
        throw std::runtime_error(error_message);
    }
    std::cout << "[Database] Table 'users' initialized." << std::endl;

    rc = sqlite3_exec(db_, create_messages_table_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error_message = "[Database] SQL error creating/checking 'messages' table: " + std::string(err_msg);
        sqlite3_free(err_msg);
        throw std::runtime_error(error_message);
    }
    std::cout << "[Database] Table 'messages' initialized." << std::endl;
}


void DatabaseManager::register_user(const std::string& username, const std::string& ip_address, unsigned short port) {
    sqlite3_stmt* stmt = nullptr;
    const std::string sql =
        "INSERT OR REPLACE INTO users (username, ip_address, port, last_connected_at) "
        "VALUES (?, ?, ?, ?);";

    std::string current_time = get_current_timestamp();
    std::lock_guard<std::mutex> lock(db_mutex_);

    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("[Database] Failed to prepare statement for user registration: " + std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, ip_address.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, port);
    sqlite3_bind_text(stmt, 4, current_time.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::string errmsg = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        throw std::runtime_error("[Database] Failed to execute user registration statement: " + errmsg);
    }

    rc = sqlite3_finalize(stmt);
     if (rc != SQLITE_OK) {
        std::cerr << "[Database] Error finalizing user registration statement: " << sqlite3_errmsg(db_) << std::endl;
    }

    std::cout << "[Database] User registered/updated: " << username << " (" << ip_address << ":" << port << ")" << std::endl;
}

void DatabaseManager::log_message(const std::string& sender_username,
                                  const std::string& recipient_username,
                                  const std::string& message_text,
                                  const std::string& sender_ip_address) {
    sqlite3_stmt* stmt = nullptr;
    const std::string sql =
        "INSERT INTO messages (sender_username, recipient_username, message_text, timestamp, sender_ip_address) "
        "VALUES (?, ?, ?, ?, ?);";

    std::string current_time = get_current_timestamp();
    std::lock_guard<std::mutex> lock(db_mutex_);

    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("[Database] Failed to prepare statement for message logging: " + std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, sender_username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, recipient_username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, message_text.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, current_time.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, sender_ip_address.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::string errmsg = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        throw std::runtime_error("[Database] Failed to execute message logging statement: " + errmsg);
    }

    rc = sqlite3_finalize(stmt);
    if (rc != SQLITE_OK) {
        std::cerr << "[Database] Error finalizing message logging statement: " << sqlite3_errmsg(db_) << std::endl;
    }
}

} // namespace tcp_messenger