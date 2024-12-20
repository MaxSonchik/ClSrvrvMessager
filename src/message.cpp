#include "../include/message.hpp"

#include <cstring>

static uint32_t read_uint32(const std::vector<uint8_t> &data, size_t &offset) {
    uint32_t val = 0;
    for (int i = 0; i < 4; i++) {
        val |= ((uint32_t)data[offset++] << (i * 8));
    }
    return val;
}

static void write_uint32(std::vector<uint8_t> &data, uint32_t val) {
    for (int i = 0; i < 4; i++) {
        data.push_back((val >> (i * 8)) & 0xFF);
    }
}

static void write_string(std::vector<uint8_t> &data, const std::string &s) {
    write_uint32(data, (uint32_t)s.size());
    data.insert(data.end(), s.begin(), s.end());
}

static std::string read_string(const std::vector<uint8_t> &data, size_t &offset) {
    uint32_t size = read_uint32(data, offset);
    std::string s(size, '\0');
    for (uint32_t i = 0; i < size; i++) {
        s[i] = (char)data[offset++];
    }
    return s;
}

std::vector<uint8_t> serialize_message(const Message &msg) {
    // format:
    // [type(1 byte)]
    // sender(str), receiver(str), text(str), filename(str), password(str)
    // file_size(8 bytes)
    std::vector<uint8_t> d;
    d.push_back((uint8_t)msg.type);
    write_string(d, msg.sender);
    write_string(d, msg.receiver);
    write_string(d, msg.text);
    write_string(d, msg.filename);
    write_string(d, msg.password);

    uint64_t fs = msg.file_size;
    for (int i = 0; i < 8; i++) {
        d.push_back((uint8_t)(fs & 0xFF));
        fs >>= 8;
    }

    std::vector<uint8_t> result;
    write_uint32(result, (uint32_t)d.size());
    result.insert(result.end(), d.begin(), d.end());
    return result;
}

Message deserialize_message(const std::vector<uint8_t> &data) {
    Message msg;
    size_t offset = 0;

    uint8_t t = data[offset++];
    msg.type = (MessageType)t;

    msg.sender = read_string(data, offset);
    msg.receiver = read_string(data, offset);
    msg.text = read_string(data, offset);
    msg.filename = read_string(data, offset);
    msg.password = read_string(data, offset);

    uint64_t fs = 0;
    for (int i = 0; i < 8; i++) {
        fs |= ((uint64_t)data[offset++] << (i * 8));
    }
    msg.file_size = fs;

    return msg;
}
