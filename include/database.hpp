#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <string>
#include <cstdint>
#include <stdexcept>
#include <sqlite3.h>

/**
 * @class Database
 * @brief Класс для управления базой данных SQLite.
 *
 * Этот класс предоставляет методы для инициализации базы данных, добавления пользователей,аутентификации и обновления информации о пользователях,проверки существования пользователя.
 */
class Database {
public:
    /**
     * @brief Конструктор для создания объекта базы данных.
     *
     * Создает подключение к базе данных SQLite, расположенной по указанному пути.
     *
     * @param db_path Путь к файлу базы данных. Тип: std::string.

     */
    Database(const std::string &db_path);
    /**
     * @brief Деструктор для освобождения ресурсов.
     *
     * Закрывает подключение к базе данных SQLite.
     */
    ~Database();
/**
     * @brief Инициализирует базу данных.
     *
     * Создает таблицы и выполняет начальную настройку базы данных, если они отсутствуют.
     */
    void init();
    /**
     * @brief Добавляет пользователя в базу данных.
     *
     * Сохраняет информацию о новом пользователе в базу данных.
     *
     * @param username Имя пользователя. Тип: std::string.
     * @param password Пароль пользователя. Тип: std::string.
     * @param ip IP-адрес пользователя. Тип: std::string.
     * @param port Порт пользователя. Тип: uint16_t.
     * @return true, если аутентификация прошла успешно и данные обновлены.
     * @return false, если аутентификация не удалась.
     */
    bool add_user(const std::string &username, const std::string &password, const std::string &ip, uint16_t port);
    /**
     * @brief Аутентифицирует пользователя и обновляет его данные.
     *
     * Проверяет, существует ли пользователь с указанными именем и паролем.
     * Если аутентификация успешна, обновляет его IP-адрес и порт.
     *
     * @param username Имя пользователя. Тип: std::string.
     * @param password Пароль пользователя. Тип: std::string.
     * @param ip Новый IP-адрес пользователя. Тип: std::string.
     * @param port Новый порт пользователя. Тип: uint16_t.
     * @return true, если аутентификация прошла успешно и данные обновлены.
     * @return false, если аутентификация не удалась.
     */
    bool authenticate_and_update(const std::string &username, const std::string &password, const std::string &ip, uint16_t port);
/**
     * @brief Проверяет существование пользователя.
     *
     * Определяет, есть ли в базе данных пользователь с указанным именем.
     *
     * @param username Имя пользователя. Тип: std::string.
     * @return true, если аутентификация прошла успешно и данные обновлены.
     * @return false, если аутентификация не удалась.
     */
    bool user_exists(const std::string &username);

private:
    sqlite3 *db_;
};

#endif
