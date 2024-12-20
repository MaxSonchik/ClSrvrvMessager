#ifndef UDP_FILE_SENDER_HPP
#define UDP_FILE_SENDER_HPP

#include <boost/asio.hpp>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>

/**
 * @class UDPFileSender
 * @brief Класс для отправки файлов через протокол UDP.
 *
 * `public`:
 *
 * Конструктор класса `UDPFileSender`
 *
 * Инициализирует объект для отправки файлов через UDP. Создается сокет
 * для передачи данных, а также подготавливается контекст ввода-вывода
 *
 * @param ioc Контекст ввода-вывода для Boost.Asio. Используется для работы с сокетами.
 * Тип: boost::asio::io_context&
 *
 *
 */
class UDPFileSender {
   public:
    UDPFileSender(boost::asio::io_context &ioc);
    /**
     * @brief Отправляет файл через UDP.
     *
     * Файл делится на блоки, которые отправляются по одному через UDP.
     * После отправки каждого блока ожидается подтверждение получения.
     *
     * @param file_path Путь к файлу, который необходимо отправить. Тип: std::string.
     * @param dest_ip IP-адрес получателя. Тип: std::string.
     * @param dest_port Порт получателя. Тип: uint16_t.
     * @return Возвращает `true`, если файл был успешно отправлен. В случае ошибки возвращает `false`.
     */
    bool send_file(const std::string &file_path, const std::string &dest_ip, uint16_t dest_port);

   private:
    /**
     * @brief Отправляет один блок данных.
     *
     * Метод отправляет блок данных через UDP и ждет подтверждения получения.
     *
     * @param block_number Номер блока, который отправляется. Тип: uint32_t.
     * @param block_data Данные блока, которые необходимо отправить. Тип: std::vector<uint8_t>.
     * @param endpoint Точка назначения для отправки данных. Тип: boost::asio::ip::udp::endpoint.
     * @return Возвращает `true`, если блок был успешно отправлен и подтверждение получено.
     *         В противном случае возвращает `false`.
     */
    bool send_block(uint32_t block_number, const std::vector<uint8_t> &block_data,
                    boost::asio::ip::udp::endpoint &endpoint);
    /**
     * @brief Ожидает подтверждение получения блока.
     *
     * Метод ждет ACK-пакет от получателя с подтверждением получения блока данных.
     *
     * @param block_number Номер блока, для которого ожидается подтверждение. Тип: uint32_t.
     * @return Возвращает `true`, если подтверждение получено. В противном случае возвращает `false`.
     */
    bool wait_for_ack(uint32_t block_number);

    boost::asio::io_context &ioc_;         // Контекст ввода-вывода для Boost.Asio
    boost::asio::ip::udp::socket socket_;  // UDP-сокет для отправки данных
    std::array<uint8_t, 2048> buffer_;     // Буфер для хранения данных для отправки
};

#endif
