#include "../include/message.hpp"
#include <stdexcept>

static uint32_t read_uint32(const std::vector<uint8_t>& data, size_t& offset) {
    if (offset + 4 > data.size()) {
        throw std::runtime_error("Not enough data for uint32");
    }
    uint32_t val = 0;
    for (int i = 0; i < 4; i++) {
        val |= ((uint32_t)data[offset++] << (i * 8));
    }
    return val;
}

static void write_uint32(std::vector<uint8_t>& data, uint32_t val) {
    for (int i = 0; i < 4; i++) {
        data.push_back((val >> (i * 8)) & 0xFF);
    }
}

static std::string read_string(const std::vector<uint8_t>& data, size_t& offset) {
    uint32_t size = read_uint32(data, offset);
    if (offset + size > data.size()) {
        throw std::runtime_error("String exceeds data bounds");
    }
    std::string s(data.begin() + offset, data.begin() + offset + size);
    offset += size;
    return s;
}

static void write_string(std::vector<uint8_t>& data, const std::string& s) {
    write_uint32(data, static_cast<uint32_t>(s.size()));
    data.insert(data.end(), s.begin(), s.end());
}

std::vector<uint8_t> serialize_message(const Message& msg) {
    std::vector<uint8_t> data;
    
    // Тип сообщения
    data.push_back(static_cast<uint8_t>(msg.type));
    
    // Строковые поля
    write_string(data, msg.sender);
    write_string(data, msg.receiver);
    write_string(data, msg.text);
    write_string(data, msg.filename);
    write_string(data, msg.password);
    
    // File size (little-endian)
    uint64_t fs = msg.file_size;
    for (int i = 0; i < 8; i++) {
        data.push_back(static_cast<uint8_t>(fs & 0xFF));
        fs >>= 8;
    }
    
    return data;
}

Message deserialize_message(const std::vector<uint8_t>& data) {
    Message msg;
    size_t offset = 0;
    
    try {
        // Тип сообщения
        if (offset >= data.size()) throw std::runtime_error("No type byte");
        msg.type = static_cast<MessageType>(data[offset++]);
        
        // Строковые поля
        msg.sender = read_string(data, offset);
        msg.receiver = read_string(data, offset);
        msg.text = read_string(data, offset);
        msg.filename = read_string(data, offset);
        msg.password = read_string(data, offset);
        
        // File size
        if (offset + 8 > data.size()) throw std::runtime_error("No file size");
        msg.file_size = 0;
        for (int i = 0; i < 8; i++) {
            msg.file_size |= static_cast<uint64_t>(data[offset++]) << (i * 8);
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Deserialization failed: " + std::string(e.what()));
    }
    
    return msg;
}