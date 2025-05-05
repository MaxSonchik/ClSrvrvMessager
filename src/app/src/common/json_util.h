#ifndef COMMON_JSON_UTIL_H
#define COMMON_JSON_UTIL_H

#include <string>
#include <nlohmann/json.hpp> // Используем nlohmann/json

namespace common {
    //базовое событие
    nlohmann::json base_event(const std::string& event);
    // ---------- регистрация ----------
    nlohmann::json register_event(const std::string& username); // клиент → сервер
    nlohmann::json register_ok_event(int user_id);              // сервер → клиент

    // ---------- запрос / выдача списка пользователей ----------
    nlohmann::json get_users_event(); // клиент → сервер
    struct UserDTO { int id; std::string name; };
    nlohmann::json users_list_event(const std::vector<UserDTO>& users); // сервер → клиент
    //ошибка
    nlohmann::json error_event(const std::string& message, const std::string& original_sender = "server");

    //статус
    nlohmann::json status_event(const std::string& status, const std::string& recipient = "");

    //сообщение
    nlohmann::json message_event(const std::string& from, const std::string& to, const std::string& text);

    //Уведомление
    nlohmann::json task_notification_event(const std::string& username, int task_id, const std::string& task_name, const std::string& description);

} // namespace common

#endif // COMMON_JSON_UTIL_H