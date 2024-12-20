#ifndef COMMON_HPP
#define COMMON_HPP

#include <cstdint>
#include <string>

static const std::string STUN_SERVER = "stun.l.google.com";
static const uint16_t STUN_PORT = 19302;
static const uint16_t SERVER_PORT = 5000;

/**
 *
 * @brief Структура для хранения публичной конечной точки.
 *
 * Представляет IP-адрес и порт клиента или сервера,
 * полученные с помощью STUN.
 *
 * // std::string ip - Публичный IP-адрес конечной точки (например, "192.168.1.1")
 *
 * // uint16_t port - Публичный порт конечной точки (например, 5000)
 */
struct PublicEndpoint {
    std::string ip;
    uint16_t port;
};

/**
 * @brief Функция,которая логирует информационное сообщение.
 *
 * Используется для вывода сообщений, отражающих нормальное состояние приложения,
 * например, успешное подключение клиента или получение данных.
 *
 * @param msg Сообщение для записи в лог. Тип: std::string.
 * @details
 */
void log_info(const std::string &msg);
/**
 * @brief Функция,которая логирует сообщение об ошибке.
 *
 * Используется для записи сообщений о критических ошибках или исключениях,
 * возникающих в процессе работы приложения.
 *
 * @param msg Сообщение для записи в лог об ошибке. Тип: std::string.
 * @details
 */
void log_error(const std::string &msg);

#endif
