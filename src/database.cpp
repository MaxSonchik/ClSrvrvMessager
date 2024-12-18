#include "../include/database.hpp"
#include "../include/encryption.hpp"
#include <iostream>

static const std::string KEY = "wfgvkljsdfjvikofdsjvkdfsjvkodsfjvksdfjvkdjfkvjkdvjkcvjbikofdjhvbiursthgbiusehvjisenvsdfklsdvhjkosdfhjjksdfhjnvj"; 



Database::Database(const std::string &db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Failed to open database");
    }
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

void Database::init() {
    const char *sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "username TEXT PRIMARY KEY,"
        "password_hash TEXT,"
        "ip TEXT,"
        "port INTEGER"
        ");";
    char *errmsg = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        std::string err = errmsg ? errmsg : "unknown error";
        sqlite3_free(errmsg);
        throw std::runtime_error("Failed to create table: " + err);
    }
}

bool Database::add_user(const std::string &username, const std::string &password, const std::string &ip, uint16_t port) {
    std::string encrypted_password = xor_encrypt(password, KEY);

    const char *sql = "INSERT INTO users (username, password_hash, ip, port) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
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

bool Database::user_exists(const std::string &username) {
    const char *sql = "SELECT 1 FROM users WHERE username = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

bool Database::authenticate_and_update(const std::string &username, const std::string &password, const std::string &ip, uint16_t port) {
    // Если нет пользователя - добавляем
    if (!user_exists(username)) {
        return add_user(username, password, ip, port); 
    }

    // Иначе проверяем пароль
    const char *sel = "SELECT password_hash FROM users WHERE username = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sel, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    std::string stored_hash;
    bool user_ok = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *db_hash = sqlite3_column_text(stmt, 0);
        if (db_hash) stored_hash = (const char*)db_hash;
        user_ok = true;
    }
    sqlite3_finalize(stmt);

    if (!user_ok) return false;

    std::string decrypted = xor_decrypt(stored_hash, KEY);
    if (decrypted != password) {
        return false;
    }

    // Пароль верен, обновим ip,port
    const char *upd = "UPDATE users SET ip = ?, port = ? WHERE username = ?;";
    if (sqlite3_prepare_v2(db_, upd, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, port);
    sqlite3_bind_text(stmt, 3, username.c_str(), -1, SQLITE_TRANSIENT);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}
