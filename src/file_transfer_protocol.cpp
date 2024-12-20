#include "../include/file_transfer_protocol.hpp"

#include <arpa/inet.h>  // для htonl/ntohl

#include <cstring>

std::vector<uint8_t> serialize_data_packet(const FileDataPacket &pkt) {
    // [4 bytes: type=1][4 bytes: block_number][4 bytes: data_size][data]
    std::vector<uint8_t> out;
    uint32_t type = 1;
    out.resize(12 + pkt.data.size());
    uint32_t *p = (uint32_t *)out.data();
    p[0] = htonl(type);
    p[1] = htonl(pkt.block_number);
    p[2] = htonl((uint32_t)pkt.data.size());
    std::memcpy(out.data() + 12, pkt.data.data(), pkt.data.size());
    return out;
}

bool parse_data_packet(const std::vector<uint8_t> &buf, FileDataPacket &pkt) {
    if (buf.size() < 12) return false;
    const uint32_t *p = (const uint32_t *)buf.data();
    uint32_t type = ntohl(p[0]);
    if (type != 1) return false;
    pkt.block_number = ntohl(p[1]);
    uint32_t dsize = ntohl(p[2]);
    if (buf.size() < 12 + dsize) return false;
    pkt.data.resize(dsize);
    std::memcpy(pkt.data.data(), buf.data() + 12, dsize);
    return true;
}

std::vector<uint8_t> serialize_ack_packet(const FileAckPacket &pkt) {
    // [4 bytes: type=2][4 bytes: block_number]
    std::vector<uint8_t> out(8);
    uint32_t *p = (uint32_t *)out.data();
    p[0] = htonl((uint32_t)2);
    p[1] = htonl(pkt.block_number);
    return out;
}

bool parse_ack_packet(const std::vector<uint8_t> &buf, FileAckPacket &pkt) {
    if (buf.size() < 8) return false;
    const uint32_t *p = (const uint32_t *)buf.data();
    uint32_t type = ntohl(p[0]);
    if (type != 2) return false;
    pkt.block_number = ntohl(p[1]);
    return true;
}
