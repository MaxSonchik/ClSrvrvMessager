#ifndef FILE_TRANSFER_PROTOCOL_HPP
#define FILE_TRANSFER_PROTOCOL_HPP

#include <cstdint>
#include <string>
#include <vector>


// Формат пакета: [4 байта: тип пакета (1=DATA, 2=ACK)][4 байта номер блока][4 байта размер данных (DATA only)][данные (DATA only)]
// Отправитель шлет DATA пакеты, получатель шлет ACK с тем же номером блока.
// Если ACK не получен за timeout - отправитель повторяет отправку.
// file_chunk_size можно установить, например, 1024 байта.


enum class FilePacketType : uint32_t {
    DATA = 1,
    ACK = 2
};

struct FileDataPacket {
    uint32_t block_number;
    std::vector<uint8_t> data;
};

struct FileAckPacket {
    uint32_t block_number;
};

// Сериализация/десериализация UDP пакетов
std::vector<uint8_t> serialize_data_packet(const FileDataPacket &pkt);
bool parse_data_packet(const std::vector<uint8_t> &buf, FileDataPacket &pkt);

std::vector<uint8_t> serialize_ack_packet(const FileAckPacket &pkt);
bool parse_ack_packet(const std::vector<uint8_t> &buf, FileAckPacket &pkt);

#endif
