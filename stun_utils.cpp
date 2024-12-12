#include "stun_utils.h"
#include <iostream>
#include <vector>
#include <cstdint> // Для uint8_t

std::vector<uint8_t> StunUtils::create_binding_request() {
    std::cout << "Creating STUN binding request..." << std::endl;
    // Пример запроса (упрощённый)
    std::vector<uint8_t> request(20, 0x00);
    request[0] = 0x00; // Тип сообщения STUN (Binding Request)
    request[1] = 0x01;
    return request;
}

void StunUtils::parse_binding_response(const std::vector<uint8_t>& response, std::string& ip, int& port) {
    (void)response;
    std::cout << "Parsing STUN binding response..." << std::endl;
    // Упрощённая логика парсинга (замените на правильный STUN-протокол)
    ip = "192.168.1.1";  // Замените на реальный IP
    port = 54321;        // Замените на реальный порт
    std::cout << "Parsed IP: " << ip << ", Port: " << port << std::endl;
}
