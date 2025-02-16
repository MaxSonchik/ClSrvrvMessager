#include "../include/database.hpp"

#include <iostream>
#include <sstream>

Database::Database(const std::string& db_path) : db_(nullptr), db_path_(db_path), is_initialized_(false) { init(); }

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
    }
}

void Database::init() {
    const char* create_users_table =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT UNIQUE NOT NULL,"
        "password TEXT NOT NULL,"
        "salt TEXT NOT NULL,"  // Добавлено поле salt
        "ip TEXT,"
        "port INTEGER);";

    execute_query(create_users_table);
}

bool Database::is_initialized() const { return is_initialized_; }

bool Database::user_exists(const std::string& username) {
    check_db_connection();

    sqlite3_stmt* stmt;
    const char* query = "SELECT 1 FROM users WHERE username = ?;";

    if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Prepare failed: " + std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);

    sqlite3_finalize(stmt);
    return exists;
}

bool Database::create_user(const std::string& username, const std::string& password_hash, const std::string& salt) {
    check_db_connection();

    sqlite3_stmt* stmt;
    const char* query = "INSERT INTO users (username, password, salt) VALUES (?, ?, ?);";

    if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Prepare failed: " + std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, salt.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool Database::authenticate_user(const std::string& username, const std::string& password) {
    check_db_connection();

    sqlite3_stmt* stmt;
    const char* query = "SELECT 1 FROM users WHERE username = ? AND password = ?;";

    if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Prepare failed: " + std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);

    bool auth = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return auth;
}

bool Database::update_connection_info(const std::string& username, const std::string& ip, uint16_t port) {
    check_db_connection();

    sqlite3_stmt* stmt;
    const char* query = "UPDATE users SET ip = ?, port = ? WHERE username = ?;";

    if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Prepare failed: " + std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, port);
    sqlite3_bind_text(stmt, 3, username.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

void Database::execute_query(const std::string& query) {
    char* err_msg = nullptr;
    if (sqlite3_exec(db_, query.c_str(), nullptr, nullptr, &err_msg) != SQLITE_OK) {
        std::string error = "SQL error: " + std::string(err_msg);
        sqlite3_free(err_msg);
        throw std::runtime_error(error);
    }
}
bool Database::authenticate_and_update(const std::string& username, const std::string& password, const std::string& ip,
                                       uint16_t port) {
    check_db_connection();

    // Проверяем логин и пароль
    bool auth = authenticate_user(username, password);
    if (!auth) {
        return false;
    }

    // Обновляем IP и порт
    return update_connection_info(username, ip, port);
}
void Database::check_db_connection() const {
    if (!db_ || !is_initialized_) {
        throw std::runtime_error("Database not initialized");
    }
}