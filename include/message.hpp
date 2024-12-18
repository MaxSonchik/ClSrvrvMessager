#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <string>
#include <vector>
#include <cstdint>

enum class MessageType : uint8_t {
    ClientRegistration = 1,
    Text,
    UserStatusRequest 
};


struct Message {
    MessageType type;
    std::string sender;
    std::string receiver;
    std::string text;
    std::string filename;
    uint64_t file_size = 0;
    std::string password; // Добавляем поле для пароля
};

// serialize/deserialize объявление
std::vector<uint8_t> serialize_message(const Message &msg);
Message deserialize_message(const std::vector<uint8_t> &data);

#endif
