//database_manager.hpp

#ifndef DATABASE_MANAGER_HPP
#define DATABASE_MANAGER_HPP

#include <string>
#include <vector>
#include <optional>
#include <sqlite3.h>
#include <mutex>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept> // Добавлено для std::runtime_error

namespace tcp_messenger {

// Структура для представления задачи
struct Task {
    int id;
    std::string username;
    std::string task_name;
    std::string description;
    std::string trigger_time_iso; // ISO 8601 format (YYYY-MM-DDTHH:MM:SSZ)
    int notify_offset_minutes;
    std::string created_at_iso;
    bool is_active;
};

struct DBUser {               //  ──► новая «DTO»
    int id;
    std::string name;
};

class DatabaseManager {
public:
    explicit DatabaseManager(const std::string& db_path);
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    // --- User Management ---
    // Регистрирует нового пользователя или обновляет существующего (IP/порт)
    bool register_user(const std::string& username, const std::string& password, const std::string& ip_address, unsigned short port);
    // Проверяет учетные данные пользователя
    bool verify_user(const std::string& username, const std::string& password, std::string& out_ip, unsigned short& out_port);
    // Обновляет IP/порт пользователя при подключении после успешной аутентификации
    void update_user_connection(const std::string& username, const std::string& ip_address, unsigned short port);
    // Получает данные пользователя (если нужен IP/порт)
    std::optional<std::pair<std::string, unsigned short>> get_user_address(const std::string& username);


    void log_message(const std::string& sender_username,
                     const std::string& recipient_username,
                     const std::string& message_text,
                     const std::string& sender_ip_address);

    
    int add_user(const std::string& username);          // INSERT, вернёт id или −1
    std::vector<DBUser> get_all_users();                // SELECT * FROM users
    // Добавляет новую задачу
    int add_task(const std::string& username, const std::string& task_name, const std::string& description,
                 const std::string& trigger_time_iso, int notify_offset_minutes);
    // Получает список активных задач для пользователя
    std::vector<Task> get_active_tasks(const std::string& username);
    // Получает все активные задачи (для загрузки планировщиком при старте)
    std::vector<Task> get_all_active_tasks();
    // Помечает задачу как неактивную (выполненную/удаленную)
    bool mark_task_inactive(int task_id);
    // Удаляет старые неактивные задачи (например, старше месяца)
    int cleanup_inactive_tasks(int days_old);
    // Вспомогательная функция для парсинга времени из DD.MM.YYYY HH:MM в ISO строку
    static std::optional<std::string> parse_user_time_to_iso(const std::string& user_time_str);

private:
    void initialize_database();
    static std::string get_current_timestamp_iso(); // Используем ISO формат
    static std::string hash_password(const std::string& password);
    static bool verify_password(const std::string& password, const std::string& hash);


    sqlite3* db_ = nullptr;
    std::string db_path_;
    std::mutex db_mutex_; // Мьютекс для защиты доступа к БД

    // RAII обертка для sqlite3_stmt
    class SQLiteStatement {
    public:
        SQLiteStatement(sqlite3* db, const std::string& query) : stmt_(nullptr), db_(db) {
            if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt_, nullptr) != SQLITE_OK) {
                throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
            }
        }
        ~SQLiteStatement() {
            if (stmt_) {
                sqlite3_finalize(stmt_);
            }
        }
        sqlite3_stmt* get() { return stmt_; }
        SQLiteStatement(const SQLiteStatement&) = delete;
        SQLiteStatement& operator=(const SQLiteStatement&) = delete;
    private:
        sqlite3_stmt* stmt_;
        sqlite3* db_; // Нужно для сообщения об ошибке finalize, если понадобится
    };
};

} // namespace tcp_messenger

#endif // DATABASE_MANAGER_HPP
