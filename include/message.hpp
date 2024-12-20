#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <cstdint>
#include <string>
#include <vector>

/**
 * @enum MessageType
 * @brief Типы сообщений в системе.
 *
 * Определяет типы сообщений, которые используются в мессенджере:
 *
 * - `ClientRegistration`: Регистрация клиента.
 *
 * - `Text`: Текстовое сообщение.
 *
 * - `UserStatusRequest`: Запрос статуса пользователя.
 */
enum class MessageType : uint8_t { ClientRegistration = 1, Text, UserStatusRequest };

/**
 * @struct Message
 * @brief Структура для представления сообщения.
 *
 * Содержит информацию о типе сообщения, отправителе, получателе, тексте,
 * передаваемом файле, размере файла и дополнительных данных (например, пароль).
 *
 * - MessageType type;      Тип сообщения (например, текст или регистрация).
 *
 * - std::string sender - Имя или идентификатор отправителя.
 *
 * - std::string receiver - Имя или идентификатор получателя.
 *
 * - std::string text - Текст сообщения (если есть).
 *
 * - std::string filename - Имя файла (если сообщение связано с файлом).
 *
 * - uint64_t file_size -Размер файла в байтах (по умолчанию 0).
 *
 * - std::string password - Поле для передачи пароля
 */
struct Message {
    MessageType type;
    std::string sender;
    std::string receiver;
    std::string text;
    std::string filename;
    uint64_t file_size = 0;
    std::string password;
};
/**
 * @brief Сериализует сообщение в массив байтов.
 *
 * Преобразует структуру `Message` в формат, пригодный для передачи по сети.
 *
 * @param msg Сообщение для сериализации.
 * @return std::vector<uint8_t> Сериализованное сообщение в виде массива байтов.
 */
std::vector<uint8_t> serialize_message(const Message &msg);

/**
 * @brief Десериализует массив байтов в структуру сообщения.
 *
 * Преобразует массив байтов, полученный по сети, в структуру `Message`.
 *
 * @param data Массив байтов, содержащий сериализованное сообщение.
 * @return Message Десериализованное сообщение.
 * @throws std::runtime_error Если данные некорректны или повреждены.
 */
Message deserialize_message(const std::vector<uint8_t> &data);

#endif
