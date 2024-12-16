#include "../include/message.hpp"
#include "../include/common.hpp"

#include <vector>
#include <cstring>
#include <arpa/inet.h> // для htonl/ntohl


std::vector<uint8_t> serialize_message(const Message &msg) {
    std::vector<uint8_t> data;
    data.push_back((uint8_t)msg.type);

    auto write_str = [&](const std::string &s) {
        uint32_t size = (uint32_t)s.size();
        uint32_t size_net = htonl(size);
        const uint8_t* p = (const uint8_t*)&size_net;
        data.insert(data.end(), p, p+4);
        data.insert(data.end(), s.begin(), s.end());
    };

    write_str(msg.sender);
    write_str(msg.receiver);
    write_str(msg.text);
    write_str(msg.filename);

    uint64_t fs = msg.file_size;
    for (int i=0; i<8; i++) {
        data.push_back((uint8_t)(fs & 0xFF));
        fs >>= 8;
    }

    // Добавляем 4 байта длины всего сообщения
    std::vector<uint8_t> result;
    uint32_t total_size = (uint32_t)data.size();
    uint32_t total_size_net = htonl(total_size);
    const uint8_t* sz_ptr = (const uint8_t*)&total_size_net;
    result.insert(result.end(), sz_ptr, sz_ptr+4);
    result.insert(result.end(), data.begin(), data.end());

    return result;
}

// Убедитесь, что deserialize_message вызывается после того, как уже прочитана длина.
Message deserialize_message(const std::vector<uint8_t> &data) {
    Message msg;
    size_t offset = 0;

    auto read_uint8 = [&](uint8_t &val) {
        val = data[offset++];
    };

    auto read_uint32 = [&](uint32_t &val) {
        uint32_t tmp = 0;
        for (int i=0;i<4;i++){
            tmp |= ((uint32_t)data[offset++] << (i*8));
        }
        val = tmp;
    };

    auto read_str = [&](std::string &s) {
        uint32_t size;
        read_uint32(size);
        s.resize(size);
        for (uint32_t i=0;i<size;i++){
            s[i]=(char)data[offset++];
        }
    };

    uint8_t t;
    read_uint8(t);
    msg.type = (MessageType)t;

    read_str(msg.sender);
    read_str(msg.receiver);
    read_str(msg.text);
    read_str(msg.filename);

    uint64_t fs = 0;
    for (int i=0;i<8;i++) {
        fs |= ((uint64_t)data[offset++] << (i*8));
    }
    msg.file_size = fs;

    return msg;
}