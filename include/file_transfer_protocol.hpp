#ifndef FILE_TRANSFER_PROTOCOL_HPP
#define FILE_TRANSFER_PROTOCOL_HPP

#include <cstdint>
#include <string>
#include <vector>

// Формат пакета: [4 байта: тип пакета (1=DATA, 2=ACK)][4 байта номер блока][4 байта размер данных (DATA only)][данные
// (DATA only)] Отправитель шлет DATA пакеты, получатель шлет ACK с тем же номером блока. Если ACK не получен за timeout
// - отправитель повторяет отправку. file_chunk_size можно установить, например, 1024 байта.

/**
 * @enum FilePacketType
 * @brief Тип пакета в протоколе передачи файлов.
 *
 * Используется для различения типов пакетов:
 *
 * - `DATA` (1): Пакет с данными.
 *
 * - `ACK` (2): Пакет подтверждения.
 */
enum class FilePacketType : uint32_t { DATA = 1, ACK = 2 };
/**
 * @struct FileDataPacket
 * @brief Структура для представления пакета данных.
 *
 * Пакет содержит номер блока и сами данные, которые необходимо передать.
 *
 * ///- uint32_t block_number - Номер блока данных
 *
 * ///- std::vector<uint8_t> data - Данные блока в виде массива байтов
 */
struct FileDataPacket {
    uint32_t block_number;
    std::vector<uint8_t> data;
};

/**
 * @struct FileAckPacket
 * @brief Структура для представления пакета подтверждения (ACK).
 *
 * Пакет подтверждения содержит только номер блока, подтверждающий успешную передачу.
 *
 * /// - uint32_t block_number - Номер подтвержденного блока
 *
 *
 */
struct FileAckPacket {
    uint32_t block_number;
};

/**
 * @brief Сериализует пакет данных (`DATA`) в массив байтов.
 *
 * Преобразует структуру `FileDataPacket` в формат, пригодный для передачи по сети.
 *
 * @param pkt Пакет данных для сериализации.
 * @return std::vector<uint8_t> Сериализованный пакет в виде массива байтов.
 */
std::vector<uint8_t> serialize_data_packet(const FileDataPacket &pkt);
/**
 * @brief Парсит массив байтов в структуру пакета данных (`DATA`).
 *
 * Преобразует полученный массив байтов в структуру `FileDataPacket`.
 *
 * @param buf Буфер с данными пакета.
 * @param pkt Структура, в которую будут записаны данные.
 * @return true Если парсинг прошел успешно.
 * @return false Если данные повреждены или не соответствуют формату.
 */
bool parse_data_packet(const std::vector<uint8_t> &buf, FileDataPacket &pkt);
/**
 * @brief Сериализует пакет подтверждения (`ACK`) в массив байтов.
 *
 * Преобразует структуру `FileAckPacket` в формат, пригодный для передачи по сети.
 *
 * @param pkt Пакет подтверждения для сериализации.
 * @return std::vector<uint8_t> Сериализованный пакет в виде массива байтов.
 */
std::vector<uint8_t> serialize_ack_packet(const FileAckPacket &pkt);
/**
 * @brief Парсит массив байтов в структуру пакета подтверждения (`ACK`).
 *
 * Преобразует полученный массив байтов в структуру `FileAckPacket`.
 *
 * @param buf Буфер с данными пакета.
 * @param pkt Структура, в которую будут записаны данные.
 * @return true,если парсинг прошел успешно.
 * @return false, если данные повреждены или не соответствуют формату.
 */
bool parse_ack_packet(const std::vector<uint8_t> &buf, FileAckPacket &pkt);
#endif
