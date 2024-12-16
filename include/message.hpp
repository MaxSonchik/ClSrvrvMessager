#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <string>
#include <vector>
#include <cstdint>

enum class MessageType : uint8_t {
    ClientRegistration = 1,
    Text,
    FileOffer,       // Предложение отправить файл
    FileAccept,      // Принятие файла
    FileDataInfo     // Информация о файле
};

struct Message {
    MessageType type;
    std::string sender;
    std::string receiver;
    std::string text;       // Для текстовых сообщений или IP
    std::string filename;   // Имя файла
    uint64_t file_size = 0; // Размер файла
};

// Сериализация/десериализация
std::vector<uint8_t> serialize_message(const Message &msg);
Message deserialize_message(const std::vector<uint8_t> &data);

// Обертки для TCP: отправка с заголовком длины (4 байта длины)
#endif
