#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <string>
#include <cstdint>
#include <stdexcept>
#include <sqlite3.h>

class Database {
public:
    Database(const std::string &db_path);
    ~Database();

    // Инициализация таблицы пользователей
    void init();

    // Добавление нового пользователя (логин, пароль, IP, порт)
    bool add_user(const std::string &username, const std::string &password, const std::string &ip, uint16_t port);

    // Проверка логина/пароля. Если верно, обновить IP/порт
    bool authenticate_and_update(const std::string &username, const std::string &password, const std::string &ip, uint16_t port);

private:
    sqlite3 *db_;
};

#endif
