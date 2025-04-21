#include "common/json_util.h"

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

// Функция escape_json удалена

} // namespace common