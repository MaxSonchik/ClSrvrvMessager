#include "../include/database.hpp"
#include <memory>
#include <sqlite3.h>
#include <stdexcept>

class StatementHandler
{
    sqlite3_stmt* stmt_;

  public:
    explicit StatementHandler(sqlite3* db, const char* query) : stmt_(nullptr)
    {
	if (sqlite3_prepare_v2(db, query, -1, &stmt_, nullptr) != SQLITE_OK)
	{
	    throw std::runtime_error(sqlite3_errmsg(db));
	}
    }

    ~StatementHandler()
    {
	if (stmt_)
	    sqlite3_finalize(stmt_);
    }

    operator sqlite3_stmt*() const { return stmt_; }

    StatementHandler(const StatementHandler&) = delete;
    StatementHandler& operator=(const StatementHandler&) = delete;
};

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
    if (is_initialized_)
	return;

    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK)
    {
	std::string error = db_ ? sqlite3_errmsg(db_) : "Unknown error";
	if (db_)
	    sqlite3_close_v2(db_);
	throw std::runtime_error("Can't open database: " + error);
    }

    is_initialized_ = true;

    execute_query("CREATE TABLE IF NOT EXISTS users ("
                  "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                  "username TEXT UNIQUE NOT NULL,"
                  "password TEXT NOT NULL,"
                  "salt TEXT NOT NULL,"
                  "ip TEXT,"
                  "port INTEGER);");

    execute_query("CREATE TABLE IF NOT EXISTS messages ("
                  "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                  "sender TEXT NOT NULL,"
                  "receiver TEXT NOT NULL,"
                  "message TEXT NOT NULL,"
                  "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);");
}

bool Database::is_initialized() const { return is_initialized_; }

bool Database::user_exists(const std::string& username)
{
    check_db_connection();
    try
    {
	StatementHandler stmt(db_, "SELECT 1 FROM users WHERE username = ?;");
	sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
	return sqlite3_step(stmt) == SQLITE_ROW;
    }
    catch (const std::exception& e)
    {
	throw std::runtime_error("user_exists: " + std::string(e.what()));
    }
}

bool Database::create_user(const std::string& username, 
                          const std::string& password_hash,
                          const std::string& salt) 
{
    // Проверяем, что соль и хэш не пустые
    if (password_hash.empty() || salt.empty()) {
        throw std::runtime_error("Invalid hash or salt");
    }
    if (password_hash.empty() || salt.empty())
    {
	throw std::runtime_error("Invalid hash or salt");
    }
    check_db_connection();
    try
    {
	StatementHandler stmt(db_,
	                      "INSERT INTO users (username, password, salt) VALUES (?, ?, ?);");

	sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, salt.c_str(), -1, SQLITE_TRANSIENT);

	return sqlite3_step(stmt) == SQLITE_DONE;
    }
    catch (const std::exception& e)
    {
	throw std::runtime_error("create_user: " + std::string(e.what()));
    }
}

bool Database::authenticate_user(const std::string& username, const std::string& password)
{
    check_db_connection();
    try
    {
	StatementHandler stmt(db_, "SELECT password, salt FROM users WHERE username = ?;");

	// Привязываем имя пользователя к запросу
	sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
	    // Получаем хэш и соль из базы данных
	    const unsigned char* hash_ptr = sqlite3_column_text(stmt, 0);
	    const unsigned char* salt_ptr = sqlite3_column_text(stmt, 1);

	    std::string stored_hash(reinterpret_cast<const char*>(hash_ptr));
	    std::string stored_salt(reinterpret_cast<const char*>(salt_ptr));

	    // Генерируем хэш с использованием сохранённой соли
	    auto computed_hash = Security::generate_hash(password, stored_salt);

	    // Логирование для отладки (опционально)
	    std::cout << "[DEBUG] Stored hash: " << stored_hash << "\n";
	    std::cout << "[DEBUG] Computed hash: " << computed_hash.hash << "\n";

	    return stored_hash == computed_hash.hash;
	}
	return false; // Пользователь не найден
    }
    catch (const std::exception& e)
    {
	throw std::runtime_error("authenticate_user: " + std::string(e.what()));
    }
}

bool Database::update_connection_info(const std::string& username, const std::string& ip,
                                      std::uint16_t port)
{
    check_db_connection();
    try
    {
	StatementHandler stmt(db_, "UPDATE users SET ip = ?, port = ? WHERE username = ?;");

	sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, port);
	sqlite3_bind_text(stmt, 3, username.c_str(), -1, SQLITE_TRANSIENT);

	return sqlite3_step(stmt) == SQLITE_DONE;
    }
    catch (const std::exception& e)
    {
	throw std::runtime_error("update_connection_info: " + std::string(e.what()));
    }
}

bool Database::authenticate_and_update(const std::string& username, const std::string& password,
                                       const std::string& ip, std::uint16_t port)
{
    check_db_connection();
    if (!authenticate_user(username, password))
	return false;
    return update_connection_info(username, ip, port);
}

bool Database::save_message(const std::string& sender, const std::string& receiver,
                            const std::string& text)
{
    check_db_connection();
    try
    {
	StatementHandler stmt(db_,
	                      "INSERT INTO messages (sender, receiver, message) VALUES (?, ?, ?);");

	sqlite3_bind_text(stmt, 1, sender.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, receiver.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, text.c_str(), -1, SQLITE_TRANSIENT);

	return sqlite3_step(stmt) == SQLITE_DONE;
    }
    catch (const std::exception& e)
    {
	throw std::runtime_error("save_message: " + std::string(e.what()));
    }
}

void Database::execute_query(const std::string& query) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, query.c_str(), nullptr, nullptr, &err_msg);
    
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? "SQL error: " + std::string(err_msg) : "Unknown error";
        sqlite3_free(err_msg); // Освобождаем только здесь
        throw std::runtime_error(error);
    }
}

void Database::check_db_connection() const
{
    if (!db_ || !is_initialized_)
    {
	throw std::runtime_error("Database connection not initialized");
    }
}