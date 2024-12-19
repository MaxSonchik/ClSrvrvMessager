#ifndef STUN_HPP
#define STUN_HPP

#include <cstdint>
#include <string>
#include <vector>

// STUN Magic Cookie по RFC 5389
static const uint32_t STUN_MAGIC_COOKIE = 0x2112A442;

/**
 * @brief 
 * Формирование STUN Binding Request.
 * Возвращает полностью сформированный STUN пакет (20-байтовый заголовок + атрибуты, если будут).
 * В данном случае без атрибутов, только заголовок.
 * 
 */
std::vector<uint8_t> build_stun_binding_request();
/** 
 * Парсинг STUN Binding Success Response.
 * На вход подаётся полученный от сервера буфер данных.
 * Возвращает true, если удалось извлечь публичный IP и порт.
 * @param ip  запись IP
 * @param port  запись порта
 */
bool parse_stun_binding_response(const std::vector<uint8_t> &data, std::string &ip, uint16_t &port);

#endif