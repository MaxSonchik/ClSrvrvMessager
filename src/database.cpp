#include "../include/database.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
/*
free(): invalid pointer

Thread 1 "test" received signal SIGABRT, Aborted.
__pthread_kill_implementation (threadid=<optimized out>, signo=signo@entry=6,
    no_tid=no_tid@entry=0) at pthread_kill.c:44
44	      return INTERNAL_SYSCALL_ERROR_P (ret) ? INTERNAL_SYSCALL_ERRNO (ret) : 0;
(gdb) bt
#0  __pthread_kill_implementation (threadid=<optimized out>,
    signo=signo@entry=6, no_tid=no_tid@entry=0) at pthread_kill.c:44
#1  0x00007ffff77ad6d3 in __pthread_kill_internal (threadid=<optimized out>,
    signo=6) at pthread_kill.c:89
#2  0x00007ffff7753ba0 in __GI_raise (sig=sig@entry=6)
    at ../sysdeps/posix/raise.c:26
#3  0x00007ffff773b582 in __GI_abort () at abort.c:73
#4  0x00007ffff773c3bf in __libc_message_impl (
    fmt=fmt@entry=0x7ffff78c931f "%s\n") at ../sysdeps/posix/libc_fatal.c:134
#5  0x00007ffff77b7765 in malloc_printerr (
    str=str@entry=0x7ffff78c7100 "free(): invalid pointer") at malloc.c:5829
#6  0x00007ffff77bca24 in _int_free_check (av=<optimized out>,
    p=0x5555557abf60, size=<optimized out>) at malloc.c:4560
#7  _int_free (av=<optimized out>, p=0x5555557abf60, have_lock=0)
    at malloc.c:4692
#8  __GI___libc_free (mem=0x5555557abf70) at malloc.c:3476
#9  0x00007ffff7755e00 in __cxa_finalize (d=0x7ffff7e0f000)
    at cxa_finalize.c:97
#10 0x00007ffff7d9f088 in ?? ()
   from /usr/lib/libboost_unit_test_framework.so.1.87.0
#11 0x00007fffffffe4a0 in ?? ()
#12 0x00007ffff7fc7fd2 in _dl_call_fini (closure_map=0x7ffff7f91fc0)
    at dl-call_fini.c:43

*/
Database::Database(const std::string& db_path)
    : db_(nullptr), db_path_(db_path), is_initialized_(false)
{
    init();
}

Database::~Database()
{
    if (db_)
    {
	sqlite3_close_v2(db_);
	db_ = nullptr;
    }
}

void Database::init()
{
    // Открываем (или создаём) файл базы данных
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK)
    {
	std::string error_msg;
	if (db_)
	{
	    error_msg = sqlite3_errmsg(db_);
	    sqlite3_close_v2(db_);
	    db_ = nullptr;
	}
	else
	{
	    error_msg = "Unknown error (database handle is null)";
	}
	throw std::runtime_error("Can't open database: " + error_msg);
    }

    is_initialized_ = true;

    // Создаём таблицу пользователей
    const char* create_users_table = "CREATE TABLE IF NOT EXISTS users ("
                                     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                     "username TEXT UNIQUE NOT NULL,"
                                     "password TEXT NOT NULL,"
                                     "salt TEXT NOT NULL,"
                                     "ip TEXT,"
                                     "port INTEGER);";
    execute_query(create_users_table);

    // Создаём таблицу для истории сообщений
    const char* create_messages_table = "CREATE TABLE IF NOT EXISTS messages ("
                                        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                        "sender TEXT NOT NULL,"
                                        "receiver TEXT NOT NULL,"
                                        "message TEXT NOT NULL,"
                                        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
                                        ");";
    execute_query(create_messages_table);
}

bool Database::is_initialized() const { return is_initialized_; }

bool Database::user_exists(const std::string& username)
{
    check_db_connection();

    sqlite3_stmt* stmt = nullptr;
    const char* query = "SELECT 1 FROM users WHERE username = ?;";

    if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK)
    {
	std::string error = sqlite3_errmsg(db_);
	sqlite3_finalize(stmt);
	throw std::runtime_error("Prepare failed: " + error);
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);

    sqlite3_finalize(stmt);
    return exists;
}

bool Database::create_user(const std::string& username, const std::string& password_hash,
                           const std::string& salt)
{
    check_db_connection();

    sqlite3_stmt* stmt = nullptr;
    const char* query = "INSERT INTO users (username, password, salt) VALUES (?, ?, ?);";

    if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK)
    {
	std::string error = sqlite3_errmsg(db_);
	sqlite3_finalize(stmt);
	throw std::runtime_error("Prepare failed: " + error);
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, salt.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool Database::authenticate_user(const std::string& username, const std::string& password)
{
    check_db_connection();

    sqlite3_stmt* stmt = nullptr;
    const char* query = "SELECT 1 FROM users WHERE username = ? AND password = ?;";

    if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK)
    {
	std::string error = sqlite3_errmsg(db_);
	sqlite3_finalize(stmt);
	throw std::runtime_error("Prepare failed: " + error);
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);

    bool auth = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return auth;
}

bool Database::update_connection_info(const std::string& username, const std::string& ip,
                                      uint16_t port)
{
    check_db_connection();

    sqlite3_stmt* stmt = nullptr;
    const char* query = "UPDATE users SET ip = ?, port = ? WHERE username = ?;";

    if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK)
    {
	std::string error = sqlite3_errmsg(db_);
	sqlite3_finalize(stmt);
	throw std::runtime_error("Prepare failed: " + error);
    }

    sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, port);
    sqlite3_bind_text(stmt, 3, username.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool Database::authenticate_and_update(const std::string& username, const std::string& password,
                                       const std::string& ip, uint16_t port)
{
    check_db_connection();

    bool auth = authenticate_user(username, password);
    if (!auth)
    {
	return false;
    }

    return update_connection_info(username, ip, port);
}

bool Database::save_message(const std::string& sender, const std::string& receiver,
                            const std::string& text)
{
    check_db_connection();

    sqlite3_stmt* stmt = nullptr;
    const char* query = "INSERT INTO messages (sender, receiver, message) VALUES (?, ?, ?);";

    if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK)
    {
	std::string error = sqlite3_errmsg(db_);
	sqlite3_finalize(stmt);
	throw std::runtime_error("Prepare failed (save_message): " + error);
    }

    sqlite3_bind_text(stmt, 1, sender.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, receiver.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, text.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

void Database::execute_query(const std::string& query)
{
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, query.c_str(), nullptr, nullptr, &err_msg);

    if (rc != SQLITE_OK)
    {
	std::string error = "SQL error: " + std::string(err_msg);
	sqlite3_free(err_msg);
	throw std::runtime_error(error);
    }

    // При успехе err_msg должен быть nullptr, но на всякий случай проверяем
    if (err_msg)
    {
	sqlite3_free(err_msg);
    }
}

void Database::check_db_connection() const
{
    if (!db_ || !is_initialized_)
    {
	throw std::runtime_error("Database not initialized");
    }
}