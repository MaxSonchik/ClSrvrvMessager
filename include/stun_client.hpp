#ifndef STUN_CLIENT_HPP
#define STUN_CLIENT_HPP

#include <string>

#include "common.hpp"

/**
 * @class StunClient
 * @brief Класс для работы с STUN-сервером.
 *
 * Используется для отправки запросов на STUN-сервер и получения информации
 * о публичном IP-адресе и порте устройства, что позволяет преодолеть NAT.
 */
class StunClient {
   public:
    /**
     * @brief Конструктор класса `StunClient`.
     *
     * Инициализирует клиента с адресом и портом STUN-сервера.
     *
     * @param stun_server Адрес STUN-сервера. Тип: std::string.
     * @param stun_port Порт STUN-сервера. Тип: uint16_t.
     */
    StunClient(const std::string &stun_server, uint16_t stun_port);
    PublicEndpoint get_public_endpoint();

   private:
    std::string stun_server_;  /// Адрес STUN-сервера
    uint16_t stun_port_;       /// Порт STUN-сервера
};

#endif
