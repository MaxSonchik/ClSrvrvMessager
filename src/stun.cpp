#include "../include/stun.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <random>

// Генерируем случайный Transaction ID
static void generate_transaction_id(uint8_t tid[12]) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
    for (int i = 0; i < 3; i++) {
        uint32_t val = dist(gen);
        tid[i * 4 + 0] = (uint8_t)((val >> 24) & 0xFF);
        tid[i * 4 + 1] = (uint8_t)((val >> 16) & 0xFF);
        tid[i * 4 + 2] = (uint8_t)((val >> 8) & 0xFF);
        tid[i * 4 + 3] = (uint8_t)(val & 0xFF);
    }
}

std::vector<uint8_t> build_stun_binding_request() {
    // STUN Binding Request (без атрибутов)
    // Тип: 0x0001, Длина: 0, Magic Cookie: 0x2112A442, Transaction ID: 12 байт
    std::vector<uint8_t> buf(20, 0);
    // Тип (Binding Request)
    buf[0] = 0x00;
    buf[1] = 0x01;
    // Длина атрибутов = 0
    buf[2] = 0x00;
    buf[3] = 0x00;
    // Magic cookie
    buf[4] = 0x21;
    buf[5] = 0x12;
    buf[6] = 0xA4;
    buf[7] = 0x42;

    // Transaction ID
    uint8_t tid[12];
    generate_transaction_id(tid);
    for (int i = 0; i < 12; i++) {
        buf[8 + i] = tid[i];
    }
    return buf;
}

bool parse_stun_binding_response(const std::vector<uint8_t> &data, std::string &ip, uint16_t &port) {
    // Минимальный размер STUN-сообщения
    if (data.size() < 20) return false;

    // Читаем заголовок
    uint16_t msg_type = ((uint16_t)data[0] << 8) | data[1];
    uint16_t msg_length = ((uint16_t)data[2] << 8) | data[3];
    uint32_t magic_cookie = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) | ((uint32_t)data[6] << 8) | data[7];

    if (magic_cookie != STUN_MAGIC_COOKIE) return false;

    // Проверим тип ответа: Binding Success Response = 0x0101
    if (msg_type != 0x0101) return false;

    // Проверим, что длина атрибутов корректна
    if (data.size() < (size_t)(20 + msg_length)) return false;

    size_t pos = 20;
    size_t end = 20 + msg_length;

    bool found_address = false;

    while (pos + 4 <= end) {
        uint16_t attr_type = (data[pos] << 8) | data[pos + 1];
        uint16_t attr_len = (data[pos + 2] << 8) | data[pos + 3];
        pos += 4;

        if (pos + attr_len > end) return false;

        // Обрабатываем XOR-MAPPED-ADDRESS (0x0020)
        if (attr_type == 0x0020) {
            // Формат атрибута (RFC 5389 Section 15.2):
            //    0x00      0x01 (family)
            //    port (2 bytes)
            //    address (4 bytes)
            if (attr_len >= 8) {
                uint8_t family = data[pos + 1];
                if (family == 0x01) {
                    // IPv4
                    uint16_t xport = (data[pos + 2] << 8) | data[pos + 3];
                    uint32_t xaddr = ((uint32_t)data[pos + 4] << 24) | ((uint32_t)data[pos + 5] << 16) |
                                     ((uint32_t)data[pos + 6] << 8) | data[pos + 7];

                    // XOR port with top 16 bits of magic cookie
                    port = xport ^ (uint16_t)(STUN_MAGIC_COOKIE >> 16);

                    // XOR IP with magic cookie
                    uint32_t real_addr = xaddr ^ STUN_MAGIC_COOKIE;
                    uint8_t a = (real_addr >> 24) & 0xFF;
                    uint8_t b = (real_addr >> 16) & 0xFF;
                    uint8_t c = (real_addr >> 8) & 0xFF;
                    uint8_t d = real_addr & 0xFF;
                    ip =
                        std::to_string(a) + "." + std::to_string(b) + "." + std::to_string(c) + "." + std::to_string(d);

                    found_address = true;
                }
            }
        }

        // Выравниваем по 4 байта
        pos += attr_len;
        size_t padding = attr_len % 4;
        if (padding != 0) pos += (4 - padding);
    }

    return found_address;
}