#include "common/json_util.h"
#include "../database/database_manager.hpp"

namespace common {

using json = nlohmann::json;

json base_event(const std::string& event) {
    json j;
    j["event"] = event;
    return j;
}

json error_event(const std::string& message, const std::string& original_sender) {
    json j = base_event("error");
    j["from"] = original_sender; // От кого исходное сообщение, вызвавшее ошибку
    j["text"] = message;
    return j;
}

json status_event(const std::string& status, const std::string& recipient) {
    json j = base_event("status");
    j["from"] = "server";
    if (!recipient.empty()) {
        j["to"] = recipient;
    }
    j["text"] = status;
    return j;
}

json message_event(const std::string& from, const std::string& to, const std::string& text) {
    json j = base_event("message");
    j["from"] = from;
    j["to"] = to;
    j["text"] = text;
    return j;
}

json task_notification_event(const std::string& username, int task_id, const std::string& task_name, const std::string& description) {
    json j = base_event("task_notification");
    j["to"] = username; // Уведомление конкретному пользователю
    j["task_id"] = task_id;
    j["task_name"] = task_name;
    j["description"] = description;
    return j;
}

// ---------- регистрация ----------
nlohmann::json register_event(const std::string& username)
{
    auto j = base_event("register");
    j["username"] = username;
    return j;
}

nlohmann::json register_ok_event(int user_id)
{
    auto j = base_event("register_ok");
    j["user_id"] = user_id;
    return j;
}

// ---------- get_users / users_list ----------
nlohmann::json get_users_event()
{
    return base_event("get_users");
}

nlohmann::json users_list_event(const std::vector<UserDTO>& users)
{
    auto j = base_event("users_list");
    j["users"] = nlohmann::json::array();
    for (const auto& u : users)
        j["users"].push_back({ {"id", u.id}, {"name", u.name} });
    return j;
}

nlohmann::json history_event(const std::vector<tcp_messenger::MessageRow>& rows)
{
    nlohmann::json j = common::base_event("history");
    j["messages"] = nlohmann::json::array();
    for (auto const& r : rows) {
        j["messages"].push_back(
            {{"from", r.from}, {"to", r.to},
             {"text", r.text}, {"ts", r.ts_iso}});
    }
    return j;
}

// Функция escape_json удалена

} // namespace common