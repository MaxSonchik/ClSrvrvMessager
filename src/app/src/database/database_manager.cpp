#include "database_manager.hpp"
#include <argon2.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <optional>
#include <random> // Для безопасной генерации соли
#include <limits> // Для numeric_limits

// Константы для Argon2
#define HASHLEN 32
#define SALTLEN 16

// Максимальная длина закодированной строки хеша (для буфера)
#define ENCODED_LEN 128

namespace tcp_messenger {

// --- Вспомогательные функции ---

std::string DatabaseManager::get_current_timestamp_iso() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

// --- ИСПРАВЛЕНО: Хеширование пароля с использованием argon2id_hash_encoded ---
std::string DatabaseManager::hash_password(const std::string& password) {
    uint8_t salt[SALTLEN];

    // --- БЕЗОПАСНАЯ Генерация соли ---
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, std::numeric_limits<uint8_t>::max());
    for (int i = 0; i < SALTLEN; ++i) {
        salt[i] = static_cast<uint8_t>(distrib(gen));
    }
    // --- Конец безопасной генерации соли ---

    // Параметры Argon2 (можно настроить)
    uint32_t t_cost = 2;         // Рекомендуется >= 1
    uint32_t m_cost = (1 << 16); // 65536 KiB = 64 MiB, рекомендуется >= 16384
    uint32_t parallelism = 1;    // Количество потоков, рекомендуется 1 или 2

    char encoded[ENCODED_LEN]; // Буфер для закодированной строки

    int result = argon2id_hash_encoded(t_cost, m_cost, parallelism,
                                       password.c_str(), password.length(),
                                       salt, SALTLEN,
                                       HASHLEN, // Длина выходного хеша в байтах
                                       encoded, ENCODED_LEN); // Буфер и его размер

    if (result != ARGON2_OK) {
        throw std::runtime_error("Failed to hash password: " + std::string(argon2_error_message(result)));
    }

    return std::string(encoded); // Возвращаем закодированную строку
}

// --- ИСПРАВЛЕНО: Проверка пароля с использованием argon2id_verify ---
bool DatabaseManager::verify_password(const std::string& password, const std::string& encoded_hash_from_db) {
    if (password.empty() || encoded_hash_from_db.empty()) {
         std::cerr << "[Database] Verify password called with empty password or hash." << std::endl;
         return false;
    }
    // encoded_hash_from_db должна быть строкой вида $argon2id$v=19$m=...,t=...,p=...$SALT$HASH
    int result = argon2id_verify(encoded_hash_from_db.c_str(), // Закодированный хеш из БД
                                 password.c_str(),          // Пароль для проверки
                                 password.length());        // Длина пароля

    if (result == ARGON2_OK) {
        return true; // Пароль совпадает
    } else if (result == ARGON2_VERIFY_MISMATCH) {
        // Это не ошибка, просто пароль не совпал. Не логируем как ошибку.
        // std::cout << "[Database DEBUG] Password mismatch for hash starting with: " << encoded_hash_from_db.substr(0, 30) << "..." << std::endl;
        return false; // Пароль не совпадает
    } else {
        // Ошибка во время верификации (неверный формат хеша, нехватка памяти и т.д.)
        std::cerr << "[Database] Error during password verification: " << argon2_error_message(result)
                  << " (Encoded hash might be invalid: " << encoded_hash_from_db.substr(0, 30) << "...)" << std::endl;
        return false; // Ошибка при проверке
    }
}

// Парсинг времени из "DD.MM.YYYY HH:MM" в ISO 8601 UTC
/*static*/ std::optional<std::string> DatabaseManager::parse_user_time_to_iso(const std::string& user_time_str) {
    std::tm tm = {};
    std::stringstream ss_parse(user_time_str);
    // Локаль может влиять на get_time, установим стандартную "C" для надежности парсинга
    ss_parse.imbue(std::locale::classic());
    ss_parse >> std::get_time(&tm, "%d.%m.%Y %H:%M");

    if (ss_parse.fail() || !ss_parse.eof()) { // Добавили проверку, что вся строка разобрана
        std::cerr << "[TimeParse] Failed to parse date/time string: " << user_time_str << std::endl;
        return std::nullopt; // Ошибка парсинга или остались лишние символы
    }

    // Важно: mktime использует локальный часовой пояс. Нам нужен UTC.
    // timegm не является стандартной функцией C++.
    // Корректный способ - использовать C++20 chrono или Boost.DateTime,
    // но для простоты пока сделаем грубое преобразование.
    // Считаем, что tm содержит время в локальном поясе сервера, преобразуем в time_t
    tm.tm_isdst = -1; // Позволяем mktime определить DST
    std::time_t time = std::mktime(&tm);

    if (time == -1) {
         std::cerr << "[TimeParse] mktime failed for parsed time: " << user_time_str << std::endl;
         return std::nullopt; // Некорректная дата/время
    }

    // Преобразуем time_t в UTC tm структуру и форматируем
    std::stringstream ss_format;
    // std::gmtime НЕ потокобезопасен. Используем gmtime_r или аналог если возможно.
    // Для простоты пока оставляем std::gmtime
    std::tm utc_tm;
    // Проверим результат gmtime
    if (gmtime_r(&time, &utc_tm) == nullptr) { // Используем потокобезопасную версию, если есть
        std::cerr << "[TimeParse] gmtime_r failed for time_t: " << time << std::endl;
        // Попробуем непотокобезопасную версию как fallback (менее предпочтительно)
        std::tm* temp_utc_tm = std::gmtime(&time);
        if (!temp_utc_tm) {
             std::cerr << "[TimeParse] std::gmtime also failed." << std::endl;
             return std::nullopt;
        }
        utc_tm = *temp_utc_tm; // Копируем результат
    }

    ss_format.imbue(std::locale::classic()); // Используем "C" локаль для форматирования
    ss_format << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");

    // Дополнительная проверка на случай ошибок форматирования
    if (ss_format.fail()) {
        std::cerr << "[TimeParse] Failed to format UTC time string." << std::endl;
        return std::nullopt;
    }

    return ss_format.str();
}


// --- Конструктор и Деструктор ---

DatabaseManager::DatabaseManager(const std::string& db_path) : db_path_(db_path) {
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_URI;
    // Добавление SQLITE_OPEN_URI позволяет использовать параметры, например ?cache=shared
    // FULLMUTEX делает SQLite потокобезопасным на уровне вызовов API,
    // но для гарантии консистентности при нескольких операциях лучше использовать внешний mutex.

    // Включим WAL режим для лучшей производительности при одновременном чтении/записи
    // Делаем это до основной инициализации таблиц
    int rc = sqlite3_open_v2(db_path_.c_str(), &db_, flags, nullptr);
    if (rc != SQLITE_OK) {
        std::string errmsg = db_ ? sqlite3_errmsg(db_) : "SQLite failed to allocate memory";
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Cannot open/create database '" + db_path + "': " + errmsg);
    }
    std::cout << "[Database] Opened successfully: " << db_path_ << std::endl;

    // Попытка включить WAL режим
    char* err_msg_wal = nullptr;
    rc = sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &err_msg_wal);
    if (rc != SQLITE_OK) {
        std::cerr << "[Database] Warning: Failed to set WAL journal mode: " << (err_msg_wal ? err_msg_wal : "Unknown error") << std::endl;
        if (err_msg_wal) sqlite3_free(err_msg_wal);
    } else {
        std::cout << "[Database] WAL journal mode enabled." << std::endl;
    }


    // Включаем поддержку внешних ключей (важно для FOREIGN KEY)
    char* err_msg_fk = nullptr;
    rc = sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &err_msg_fk);
     if (rc != SQLITE_OK) {
        std::cerr << "[Database] Warning: Failed to enable foreign key support: " << (err_msg_fk ? err_msg_fk : "Unknown error") << std::endl;
        if (err_msg_fk) sqlite3_free(err_msg_fk);
    } else {
        std::cout << "[Database] Foreign key support enabled." << std::endl;
    }


    try {
        initialize_database();
    } catch (...) {
        sqlite3_close(db_); // Закрываем БД перед перебросом исключения
        db_ = nullptr;
        throw;
    }
}

DatabaseManager::~DatabaseManager() {
    if (db_) {
        // Попытка закрыть с проверкой на незавершенные транзакции (может занять время)
        int rc = sqlite3_close_v2(db_); // Используем _v2 для лучшей диагностики
        if (rc == SQLITE_BUSY) {
             std::cerr << "[Database] Error closing: Database is busy (unfinalized statements?). Retrying close..." << std::endl;
             // Можно добавить цикл с finalize всех активных стейтментов, если они отслеживаются
             // Или просто использовать sqlite3_close() как fallback
             rc = sqlite3_close(db_); // Простая попытка закрытия
        }

        if (rc != SQLITE_OK) {
            // Ошибка даже после второй попытки
            std::cerr << "[Database] Error closing database definitely: (" << rc << ") " << sqlite3_errmsg(db_) << std::endl;
            // Не бросаем исключение из деструктора
        } else {
            std::cout << "[Database] Closed: " << db_path_ << std::endl;
        }
        db_ = nullptr; // Указываем, что БД закрыта
    }
}

// --- Инициализация БД ---

void DatabaseManager::initialize_database() {
    // SQL запросы оставляем как были
    const char* create_users_table_sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "username TEXT PRIMARY KEY NOT NULL,"
        "password_hash TEXT NOT NULL,"
        "ip_address TEXT,"
        "port INTEGER,"
        "last_connected_at TEXT" // ISO 8601 format
        ");";
    const char* create_messages_table_sql =
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "sender_username TEXT NOT NULL,"
        "recipient_username TEXT NOT NULL,"
        "message_text TEXT,"
        "timestamp TEXT NOT NULL," // ISO 8601 format
        "sender_ip_address TEXT,"
        "FOREIGN KEY(sender_username) REFERENCES users(username) ON DELETE CASCADE," // Добавим ON DELETE CASCADE
        "FOREIGN KEY(recipient_username) REFERENCES users(username) ON DELETE SET NULL" // Или CASCADE? Зависит от логики
        ");";
     const char* create_tasks_table_sql =
        "CREATE TABLE IF NOT EXISTS tasks ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT NOT NULL,"
        "task_name TEXT NOT NULL," // Сделаем имя обязательным
        "description TEXT,"
        "trigger_time TEXT NOT NULL," // ISO 8601 format (UTC)
        "notify_offset_minutes INTEGER DEFAULT 0,"
        "created_at TEXT NOT NULL," // ISO 8601 format (UTC)
        "is_active BOOLEAN DEFAULT 1,"
        "FOREIGN KEY(username) REFERENCES users(username) ON DELETE CASCADE" // Задачи удаляются при удалении пользователя
        ");";
    // Индексы для ускорения поиска
    const char* index_tasks_user_active = "CREATE INDEX IF NOT EXISTS idx_tasks_user_active ON tasks(username, is_active);";
    const char* index_tasks_active_trigger = "CREATE INDEX IF NOT EXISTS idx_tasks_active_trigger ON tasks(is_active, trigger_time);"; // Для планировщика
    const char* index_messages_recipient = "CREATE INDEX IF NOT EXISTS idx_messages_recipient ON messages(recipient_username);"; // Для выборки оффлайн сообщений


    char* err_msg = nullptr;
    std::lock_guard<std::mutex> lock(db_mutex_); // Блокируем на время выполнения DDL

    auto execute_sql = [&](const char* sql, const std::string& description) {
        int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            std::string error_message = "[Database] SQL error executing " + description + ": " + (err_msg ? err_msg : sqlite3_errmsg(db_));
            if (err_msg) sqlite3_free(err_msg);
            err_msg = nullptr;
            throw std::runtime_error(error_message);
        }
        std::cout << "[Database] Executed: " << description << "." << std::endl;
         if (err_msg) sqlite3_free(err_msg); // На всякий случай
         err_msg = nullptr;
    };

    execute_sql(create_users_table_sql, "CREATE users table");
    execute_sql(create_messages_table_sql, "CREATE messages table");
    execute_sql(create_tasks_table_sql, "CREATE tasks table");
    execute_sql(index_tasks_user_active, "CREATE tasks user/active index");
    execute_sql(index_tasks_active_trigger, "CREATE tasks active/trigger index");
    execute_sql(index_messages_recipient, "CREATE messages recipient index");

    // PRAGMA foreign_keys = ON; уже выполнен в конструкторе
}


// --- User Management ---

bool DatabaseManager::register_user(const std::string& username, const std::string& password, const std::string& ip_address, unsigned short port) {
    if (username.empty() || password.empty()) {
        std::cerr << "[Database] Registration failed: Username or password cannot be empty." << std::endl;
        return false;
    }
    // Можно добавить проверку длины/формата имени и пароля

    const std::string sql_check = "SELECT 1 FROM users WHERE username = ?;";
    const std::string sql_insert =
        "INSERT INTO users (username, password_hash, ip_address, port, last_connected_at) "
        "VALUES (?, ?, ?, ?, ?);";

    std::lock_guard<std::mutex> lock(db_mutex_);

    // 1. Проверяем, существует ли пользователь
    try {
        SQLiteStatement stmt_check(db_, sql_check);
        sqlite3_bind_text(stmt_check.get(), 1, username.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt_check.get()) == SQLITE_ROW) {
            std::cerr << "[Database] Registration failed: Username '" << username << "' already exists." << std::endl;
            return false; // Пользователь уже существует
        }
    } catch (const std::exception& e) {
        std::cerr << "[Database] Error checking existing user '" << username << "': " << e.what() << std::endl;
        return false; // Ошибка при проверке
    }


    // 2. Хешируем пароль
    std::string password_hash;
    try {
        password_hash = hash_password(password);
    } catch (const std::exception& e) {
        std::cerr << "[Database] Failed to hash password for user '" << username << "': " << e.what() << std::endl;
        return false; // Ошибка хеширования
    }

    // 3. Вставляем нового пользователя
    try {
         SQLiteStatement stmt_insert(db_, sql_insert);
         std::string current_time = get_current_timestamp_iso();

         sqlite3_bind_text(stmt_insert.get(), 1, username.c_str(), -1, SQLITE_STATIC);
         // Используем SQLITE_TRANSIENT, так как password_hash - локальная строка,
         // которая может быть уничтожена до завершения выполнения запроса SQLite.
         sqlite3_bind_text(stmt_insert.get(), 2, password_hash.c_str(), -1, SQLITE_TRANSIENT);
         sqlite3_bind_text(stmt_insert.get(), 3, ip_address.c_str(), -1, SQLITE_STATIC);
         sqlite3_bind_int(stmt_insert.get(), 4, port);
         sqlite3_bind_text(stmt_insert.get(), 5, current_time.c_str(), -1, SQLITE_STATIC);

         int rc = sqlite3_step(stmt_insert.get());
         if (rc != SQLITE_DONE) {
            std::cerr << "[Database] Failed to execute user registration statement for '" << username << "': (" << rc << ") " << sqlite3_errmsg(db_) << std::endl;
            return false; // Ошибка вставки
         }
    } catch (const std::exception& e) {
         std::cerr << "[Database] Error inserting new user '" << username << "': " << e.what() << std::endl;
         return false; // Ошибка подготовки/выполнения
    }

    std::cout << "[Database] User registered: " << username << std::endl;
    return true;
}

// Проверка имени пользователя и пароля
bool DatabaseManager::verify_user(const std::string& username, const std::string& password, std::string& out_ip, unsigned short& out_port) {
    if (username.empty() || password.empty()) {
        return false; // Не пытаемся проверять пустые данные
    }
    const std::string sql = "SELECT password_hash, ip_address, port FROM users WHERE username = ?;";
    std::lock_guard<std::mutex> lock(db_mutex_);

    try {
        SQLiteStatement stmt(db_, sql);
        sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_STATIC);

        int rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_ROW) {
            // Пользователь найден, проверяем хеш
            const unsigned char* stored_encoded_hash_ptr = sqlite3_column_text(stmt.get(), 0);
            const unsigned char* ip_ptr = sqlite3_column_text(stmt.get(), 1);
            int port_val = sqlite3_column_int(stmt.get(), 2);

            if (!stored_encoded_hash_ptr) {
                 std::cerr << "[Database] User '" << username << "' found but password hash is NULL in DB." << std::endl;
                 return false; // Данные в БД повреждены или некорректны
            }

            // Копируем строку хеша из результата запроса
            std::string stored_encoded_hash(reinterpret_cast<const char*>(stored_encoded_hash_ptr));

            // Вызываем исправленную verify_password
            if (verify_password(password, stored_encoded_hash)) {
                 // Пароль верный, получаем IP и порт
                 out_ip = (ip_ptr ? reinterpret_cast<const char*>(ip_ptr) : "");
                 out_port = (port_val > 0 && port_val <= 65535) ? static_cast<unsigned short>(port_val) : 0;
                 // std::cout << "[Database] User '" << username << "' verified successfully." << std::endl; // Можно убрать для меньшего логгирования
                 return true;
            } else {
                 // Пароль неверный
                 std::cout << "[Database] Password verification failed for user '" << username << "'." << std::endl;
                 return false;
            }
        } else if (rc == SQLITE_DONE) {
             // Пользователь не найден
             std::cout << "[Database] User '" << username << "' not found during verification." << std::endl;
            return false;
        } else {
             // Ошибка выполнения запроса
             std::cerr << "[Database] Failed to execute user verification statement for '" << username << "': (" << rc << ") " << sqlite3_errmsg(db_) << std::endl;
            return false;
        }
         // finalize вызывается в деструкторе stmt
    } catch (const std::exception& e) {
         // Ошибка подготовки запроса
         std::cerr << "[Database] Exception during user verification for '" << username << "': " << e.what() << std::endl;
         return false;
    }
}


// Обновление данных подключения пользователя
void DatabaseManager::update_user_connection(const std::string& username, const std::string& ip_address, unsigned short port) {
     if (username.empty()) return; // Не обновляем безымянного пользователя

     const std::string sql = "UPDATE users SET ip_address = ?, port = ?, last_connected_at = ? WHERE username = ?;";
     std::lock_guard<std::mutex> lock(db_mutex_);

     try {
         SQLiteStatement stmt(db_, sql);
         std::string current_time = get_current_timestamp_iso();

         // Привязываем параметры
         sqlite3_bind_text(stmt.get(), 1, ip_address.c_str(), -1, SQLITE_STATIC);
         sqlite3_bind_int(stmt.get(), 2, port);
         sqlite3_bind_text(stmt.get(), 3, current_time.c_str(), -1, SQLITE_STATIC);
         sqlite3_bind_text(stmt.get(), 4, username.c_str(), -1, SQLITE_STATIC);

         // Выполняем запрос
         int rc = sqlite3_step(stmt.get());
         if (rc != SQLITE_DONE) {
              // Ошибка обновления
              std::cerr << "[Database] Failed to update user connection info for '" << username << "': (" << rc << ") " << sqlite3_errmsg(db_) << std::endl;
         } else {
              // Успешно, проверяем, сколько строк было затронуто
              if (sqlite3_changes(db_) > 0) {
                 // std::cout << "[Database] User connection updated: " << username << " (" << ip_address << ":" << port << ")" << std::endl; // Можно убрать для тишины
              } else {
                 // Пользователь не найден (маловероятно, если вызов после verify_user)
                 std::cerr << "[Database] Failed to update connection for user '" << username << "': User not found." << std::endl;
              }
         }
         // finalize в деструкторе stmt
     } catch (const std::exception& e) {
         // Ошибка подготовки запроса
         std::cerr << "[Database] Exception during user connection update for '" << username << "': " << e.what() << std::endl;
     }
}

// Получение адреса пользователя (например, для P2P)
std::optional<std::pair<std::string, unsigned short>> DatabaseManager::get_user_address(const std::string& username) {
    if (username.empty()) return std::nullopt;

    const std::string sql = "SELECT ip_address, port FROM users WHERE username = ?;";
    std::lock_guard<std::mutex> lock(db_mutex_);

    try {
        SQLiteStatement stmt(db_, sql);
        sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_STATIC);

        int rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_ROW) {
            const unsigned char* ip_ptr = sqlite3_column_text(stmt.get(), 0);
            int port_val = sqlite3_column_int(stmt.get(), 1);
            // Проверяем, что IP и порт не NULL и корректны
            if (ip_ptr && port_val > 0 && port_val <= 65535) {
                std::string ip(reinterpret_cast<const char*>(ip_ptr));
                 if (!ip.empty()) { // Дополнительная проверка на пустую строку IP
                    return std::make_pair(ip, static_cast<unsigned short>(port_val));
                 }
            }
             // Адрес не найден или некорректен
             std::cout << "[Database DEBUG] Address not found or invalid for user '" << username << "'." << std::endl;
             return std::nullopt;
        } else if (rc == SQLITE_DONE) {
             // Пользователь не найден
             return std::nullopt;
        } else {
             // Ошибка SQL
             std::cerr << "[Database] Failed to get user address for '" << username << "': (" << rc << ") " << sqlite3_errmsg(db_) << std::endl;
             return std::nullopt;
        }
        // finalize в деструкторе stmt
    } catch (const std::exception& e) {
         std::cerr << "[Database] Exception getting user address for '" << username << "': " << e.what() << std::endl;
         return std::nullopt;
    }
}


// --- Message Logging ---

void DatabaseManager::log_message(const std::string& sender_username,
                                  const std::string& recipient_username,
                                  const std::string& message_text,
                                  const std::string& sender_ip_address) {
    if (sender_username.empty() || recipient_username.empty()) return;

    const std::string sql =
        "INSERT INTO messages (sender_username, recipient_username, message_text, timestamp, sender_ip_address) "
        "VALUES (?, ?, ?, ?, ?);";
    std::lock_guard<std::mutex> lock(db_mutex_);

    try {
        SQLiteStatement stmt(db_, sql);
        std::string current_time = get_current_timestamp_iso();

        // Привязываем параметры
        sqlite3_bind_text(stmt.get(), 1, sender_username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt.get(), 2, recipient_username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt.get(), 3, message_text.c_str(), -1, SQLITE_STATIC); // Сообщение может быть большим, но SQLite справится
        sqlite3_bind_text(stmt.get(), 4, current_time.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt.get(), 5, sender_ip_address.c_str(), -1, SQLITE_STATIC);

        // Выполняем
        int rc = sqlite3_step(stmt.get());
        if (rc != SQLITE_DONE) {
             std::cerr << "[Database] Failed to execute message logging statement: (" << rc << ") " << sqlite3_errmsg(db_) << std::endl;
        }
        // finalize в деструкторе stmt
    } catch (const std::exception& e) {
        std::cerr << "[Database] Exception logging message from '" << sender_username << "' to '" << recipient_username << "': " << e.what() << std::endl;
    }
}

// --- Task Management ---

// Добавление новой задачи
int DatabaseManager::add_task(const std::string& username, const std::string& task_name, const std::string& description,
                              const std::string& trigger_time_iso, int notify_offset_minutes) {
    // Проверки входных данных
    if (username.empty() || task_name.empty() || trigger_time_iso.empty() || notify_offset_minutes < 0) {
         std::cerr << "[Database] Invalid arguments for add_task." << std::endl;
         return -1;
    }

    const std::string sql =
        "INSERT INTO tasks (username, task_name, description, trigger_time, notify_offset_minutes, created_at, is_active) "
        "VALUES (?, ?, ?, ?, ?, ?, 1);";
    std::lock_guard<std::mutex> lock(db_mutex_);
    long long last_id = -1; // Используем long long для rowid

     try {
         SQLiteStatement stmt(db_, sql);
         std::string current_time = get_current_timestamp_iso();

         // Привязываем параметры
         sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_STATIC);
         sqlite3_bind_text(stmt.get(), 2, task_name.c_str(), -1, SQLITE_STATIC);
         sqlite3_bind_text(stmt.get(), 3, description.c_str(), -1, SQLITE_STATIC);
         sqlite3_bind_text(stmt.get(), 4, trigger_time_iso.c_str(), -1, SQLITE_STATIC);
         sqlite3_bind_int(stmt.get(), 5, notify_offset_minutes);
         sqlite3_bind_text(stmt.get(), 6, current_time.c_str(), -1, SQLITE_STATIC);

         // Выполняем
         int rc = sqlite3_step(stmt.get());
         if (rc == SQLITE_DONE) {
             last_id = sqlite3_last_insert_rowid(db_);
             // std::cout << "[Database] Task added for user '" << username << "' with ID: " << last_id << std::endl;
         } else {
              std::cerr << "[Database] Failed to execute add task statement for user '" << username << "': (" << rc << ") " << sqlite3_errmsg(db_) << std::endl;
         }
         // finalize в деструкторе stmt
     } catch (const std::exception& e) {
         std::cerr << "[Database] Exception adding task for user '" << username << "': " << e.what() << std::endl;
     }
     // Преобразуем long long в int, если ID не слишком большой (для SQLite обычно так)
     if (last_id >= std::numeric_limits<int>::min() && last_id <= std::numeric_limits<int>::max()) {
        return static_cast<int>(last_id);
     } else {
        return -1; // Ошибка или слишком большой ID
     }
}

// Получение активных задач для пользователя
std::vector<Task> DatabaseManager::get_active_tasks(const std::string& username) {
     if (username.empty()) return {};

     const std::string sql =
        "SELECT id, username, task_name, description, trigger_time, notify_offset_minutes, created_at, is_active "
        "FROM tasks WHERE username = ? AND is_active = 1 ORDER BY trigger_time ASC;";
     std::vector<Task> tasks;
     std::lock_guard<std::mutex> lock(db_mutex_);

     try {
        SQLiteStatement stmt(db_, sql);
        sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_STATIC);

        int rc;
        while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
            Task task;
            task.id = sqlite3_column_int(stmt.get(), 0);
            // Проверяем указатели перед разыменованием
            const unsigned char* user_ptr = sqlite3_column_text(stmt.get(), 1);
            const unsigned char* name_ptr = sqlite3_column_text(stmt.get(), 2);
            const unsigned char* desc_ptr = sqlite3_column_text(stmt.get(), 3);
            const unsigned char* trigger_ptr = sqlite3_column_text(stmt.get(), 4);
            const unsigned char* created_ptr = sqlite3_column_text(stmt.get(), 6);

            task.username = user_ptr ? reinterpret_cast<const char*>(user_ptr) : "";
            task.task_name = name_ptr ? reinterpret_cast<const char*>(name_ptr) : "";
            task.description = desc_ptr ? reinterpret_cast<const char*>(desc_ptr) : "";
            task.trigger_time_iso = trigger_ptr ? reinterpret_cast<const char*>(trigger_ptr) : "";
            task.notify_offset_minutes = sqlite3_column_int(stmt.get(), 5);
            task.created_at_iso = created_ptr ? reinterpret_cast<const char*>(created_ptr) : "";
            task.is_active = (sqlite3_column_int(stmt.get(), 7) == 1); // is_active не может быть NULL по схеме

            // Добавляем задачу, только если все обязательные поля корректны
            if (task.id > 0 && !task.username.empty() && !task.task_name.empty() && !task.trigger_time_iso.empty()) {
                 tasks.push_back(std::move(task)); // Используем move для эффективности
            } else {
                 std::cerr << "[Database] Warning: Skipping task with invalid data during fetch (ID: " << task.id << ")" << std::endl;
            }
        }
        if (rc != SQLITE_DONE) {
             std::cerr << "[Database] Error during step in get_active_tasks for user '" << username << "': (" << rc << ") " << sqlite3_errmsg(db_) << std::endl;
        }
        // finalize в деструкторе stmt
     } catch (const std::exception& e) {
         std::cerr << "[Database] Exception getting active tasks for user '" << username << "': " << e.what() << std::endl;
     }
     return tasks;
}

// Получение всех активных задач для планировщика
std::vector<Task> DatabaseManager::get_all_active_tasks() {
    const std::string sql =
        "SELECT id, username, task_name, description, trigger_time, notify_offset_minutes, created_at, is_active "
        "FROM tasks WHERE is_active = 1;"; // Индекс idx_tasks_active_trigger должен помочь
     std::vector<Task> tasks;
     std::lock_guard<std::mutex> lock(db_mutex_);

     try {
        SQLiteStatement stmt(db_, sql);

        int rc;
        while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
             Task task;
             task.id = sqlite3_column_int(stmt.get(), 0);
             const unsigned char* user_ptr = sqlite3_column_text(stmt.get(), 1);
             const unsigned char* name_ptr = sqlite3_column_text(stmt.get(), 2);
             const unsigned char* desc_ptr = sqlite3_column_text(stmt.get(), 3);
             const unsigned char* trigger_ptr = sqlite3_column_text(stmt.get(), 4);
             const unsigned char* created_ptr = sqlite3_column_text(stmt.get(), 6);

             task.username = user_ptr ? reinterpret_cast<const char*>(user_ptr) : "";
             task.task_name = name_ptr ? reinterpret_cast<const char*>(name_ptr) : "";
             task.description = desc_ptr ? reinterpret_cast<const char*>(desc_ptr) : "";
             task.trigger_time_iso = trigger_ptr ? reinterpret_cast<const char*>(trigger_ptr) : "";
             task.notify_offset_minutes = sqlite3_column_int(stmt.get(), 5);
             task.created_at_iso = created_ptr ? reinterpret_cast<const char*>(created_ptr) : "";
             task.is_active = (sqlite3_column_int(stmt.get(), 7) == 1);

            if (task.id > 0 && !task.username.empty() && !task.task_name.empty() && !task.trigger_time_iso.empty()) {
                 tasks.push_back(std::move(task));
            } else {
                 std::cerr << "[Database] Warning: Skipping task with invalid data during fetch all (ID: " << task.id << ")" << std::endl;
            }
        }
         if (rc != SQLITE_DONE) {
             std::cerr << "[Database] Error during step in get_all_active_tasks: (" << rc << ") " << sqlite3_errmsg(db_) << std::endl;
         }
        // finalize в деструкторе stmt
     } catch (const std::exception& e) {
         std::cerr << "[Database] Exception getting all active tasks: " << e.what() << std::endl;
     }
     return tasks;
}


// Пометка задачи как неактивной
bool DatabaseManager::mark_task_inactive(int task_id) {
    if (task_id <= 0) return false;

    const std::string sql = "UPDATE tasks SET is_active = 0 WHERE id = ? AND is_active = 1;"; // Обновляем только активные
    std::lock_guard<std::mutex> lock(db_mutex_);
    bool success = false;

    try {
        SQLiteStatement stmt(db_, sql);
        sqlite3_bind_int(stmt.get(), 1, task_id);

        int rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_DONE) {
             if (sqlite3_changes(db_) > 0) { // Проверяем, была ли запись действительно обновлена
                 // std::cout << "[Database] Task ID " << task_id << " marked as inactive." << std::endl;
                 success = true;
             } else {
                 // Задача не найдена или уже была неактивна
                 // std::cout << "[Database] Task ID " << task_id << " not found or already inactive." << std::endl;
                 // Возвращаем true, если она уже неактивна (цель достигнута)
                 // Проверим отдельно
                 const std::string check_sql = "SELECT 1 FROM tasks WHERE id = ? AND is_active = 0;";
                 try {
                     SQLiteStatement check_stmt(db_, check_sql);
                     sqlite3_bind_int(check_stmt.get(), 1, task_id);
                     if (sqlite3_step(check_stmt.get()) == SQLITE_ROW) {
                         success = true; // Задача существует и уже неактивна
                     }
                 } catch (...) { /* Игнорируем ошибку проверки */ }

             }
        } else {
             std::cerr << "[Database] Failed to mark task inactive (ID: " << task_id << "): (" << rc << ") " << sqlite3_errmsg(db_) << std::endl;
        }
        // finalize в деструкторе stmt
    } catch (const std::exception& e) {
        std::cerr << "[Database] Exception marking task inactive (ID: " << task_id << "): " << e.what() << std::endl;
    }
    return success;
}

// Очистка старых неактивных задач
int DatabaseManager::cleanup_inactive_tasks(int days_old) {
    if (days_old < 0) return 0; // Не удаляем, если интервал отрицательный

    // Рассчитываем временную метку N дней назад
    auto now = std::chrono::system_clock::now();
    auto past_time = now - std::chrono::hours(24 * days_old);
    auto in_time_t = std::chrono::system_clock::to_time_t(past_time);
    std::stringstream ss;
    // Используем gmtime_r для потокобезопасности
    std::tm cutoff_tm;
    if (gmtime_r(&in_time_t, &cutoff_tm) == nullptr) {
        std::cerr << "[Database] gmtime_r failed in cleanup_inactive_tasks." << std::endl;
        return 0; // Не можем рассчитать дату
    }
    ss.imbue(std::locale::classic());
    ss << std::put_time(&cutoff_tm, "%Y-%m-%dT%H:%M:%SZ");
    std::string cutoff_timestamp = ss.str();

    const std::string sql = "DELETE FROM tasks WHERE is_active = 0 AND created_at < ?;";
    std::lock_guard<std::mutex> lock(db_mutex_);
    int deleted_count = 0;

     try {
        SQLiteStatement stmt(db_, sql);
        sqlite3_bind_text(stmt.get(), 1, cutoff_timestamp.c_str(), -1, SQLITE_STATIC);

        int rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_DONE) {
             deleted_count = sqlite3_changes(db_);
             if (deleted_count > 0) {
                std::cout << "[Database] Cleaned up " << deleted_count << " inactive tasks older than " << days_old << " days (before " << cutoff_timestamp << ")." << std::endl;
             } else {
                // std::cout << "[Database] No inactive tasks found older than " << days_old << " days to cleanup." << std::endl;
             }
        } else {
             std::cerr << "[Database] Failed to cleanup inactive tasks: (" << rc << ") " << sqlite3_errmsg(db_) << std::endl;
        }
        // finalize в деструкторе stmt
    } catch (const std::exception& e) {
        std::cerr << "[Database] Exception cleaning up inactive tasks: " << e.what() << std::endl;
    }
    return deleted_count;
}


} // namespace tcp_messenger
