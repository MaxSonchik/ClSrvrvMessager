#include "../include/database.hpp"
#include "../include/encryption.hpp"
#include <sqlite3.h>
#include <stdexcept>
#include <string>

// Простая константа ключа для XOR-"шифрования"
static const std::string KEY = "my_simple_key";

Database::Database(const std::string &db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Failed to open database: " + std::string(sqlite3_errmsg(db_)));
    }
}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
    }
}

void Database::init() {
    const char *create_table_sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "username TEXT PRIMARY KEY,"
        "password_hash TEXT,"
        "ip TEXT,"
        "port INTEGER"
        ");";

    char *errmsg = nullptr;
    if (sqlite3_exec(db_, create_table_sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        std::string err = errmsg ? errmsg : "unknown error";
        sqlite3_free(errmsg);
        throw std::runtime_error("Failed to create users table: " + err);
    }
}

bool Database::add_user(const std::string &username, const std::string &password, const std::string &ip, uint16_t port) {
    std::string encrypted_password = xor_encrypt(password, KEY);

    // Поскольку salt не используем, просто храним password_hash = encrypted_password
    const char *insert_sql = "INSERT INTO users (username, password_hash, ip, port) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, insert_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, encrypted_password.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, ip.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, port);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool Database::authenticate_and_update(const std::string &username, const std::string &password, const std::string &ip, uint16_t port) {
    const char *select_sql = "SELECT password_hash FROM users WHERE username = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, select_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    bool user_exists = false;
    std::string stored_hash;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user_exists = true;
        const unsigned char *db_hash = sqlite3_column_text(stmt, 0);
        if (db_hash) {
            stored_hash = (const char*)db_hash;
        }
    }

    sqlite3_finalize(stmt);

    if (!user_exists) {
        // Нет пользователя, добавим нового
        return add_user(username, password, ip, port);
    } else {
        // Расшифровываем hash и сравниваем с введённым паролем
        std::string decrypted_password = xor_decrypt(stored_hash, KEY);
        if (decrypted_password != password) {
            return false;
        }

        // Обновляем IP/port
        const char *update_sql = "UPDATE users SET ip = ?, port = ? WHERE username = ?;";
        if (sqlite3_prepare_v2(db_, update_sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, port);
        sqlite3_bind_text(stmt, 3, username.c_str(), -1, SQLITE_TRANSIENT);

        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }
}
