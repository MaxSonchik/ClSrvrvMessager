#ifndef COMMON_HPP
#define COMMON_HPP

#include <string>
#include <cstdint>
#include <iostream>
#include <vector>

// Константы для STUN
static const std::string STUN_SERVER = "stun.l.google.com";
static const uint16_t STUN_PORT = 19302;

// Порты для сервера и P2P передачи
static const uint16_t SERVER_PORT = 5000;
static const uint16_t FILE_TRANSFER_PORT = 6000;

// Структура для публичной точки
struct PublicEndpoint {
    std::string ip;
    uint16_t port;
};

// Логирование
void log_info(const std::string &msg);
void log_error(const std::string &msg);

// Утилита для преобразования из/в network byte order при сериализации.
inline uint32_t host_to_net32(uint32_t x) {
    return ((x>>24)&0xFF) | ((x>>8)&0xFF00) | ((x<<8)&0xFF0000) | ((x<<24)&0xFF000000);
}

inline uint32_t net_to_host32(uint32_t x) {
    return ((x>>24)&0xFF) | ((x>>8)&0xFF00) | ((x<<8)&0xFF0000) | ((x<<24)&0xFF000000);
}

#endif
