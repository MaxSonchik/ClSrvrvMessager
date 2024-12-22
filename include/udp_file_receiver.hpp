#ifndef UDP_FILE_RECEIVER_HPP
#define UDP_FILE_RECEIVER_HPP

#include <boost/asio.hpp>
#include <fstream>
#include <string>


/**
 * @brief UDPFileReceiver
 * @brief Класс для приема файлов через протокол UDP.
 *
 */
class UDPFileReceiver {
   public:
    /**
     * @brief Конструктор класса `UDPFileReceiver`.
     *
     * Инициализирует приемник с контекстом ввода-вывода, портом для приема
     * данных и путем для сохранения полученного файла.
     *
     * @param ioc Контекст ввода-вывода для Boost.Asio. Тип: boost::asio::io_context.
     * @param port Порт для прослушивания входящих сообщений. Тип: uint16_t.
     * @param save_path Путь для сохранения полученного файла. Тип: std::string.
     */
    UDPFileReceiver(boost::asio::io_context &ioc, uint16_t port, const std::string &save_path);
    /**
     * @brief Запускает прием данных.
     *
     * Метод инициализирует сокет и начинает прослушивание порта на наличие
     * входящих UDP-пакетов. После получения данных выполняет сохранение и
     * отправляет подтверждения (ACK).
     */
    void start();

   private:
    /**
     * @brief Ожидает и принимает данные.
     *
     * Этот метод обрабатывает входящие данные от отправителя, записывая
     * их в файл и отправляя подтверждение об успешной передаче.
     */
    void do_receive();
    /**
     * @brief Отправляет подтверждение получения блока данных.
     *
     * Отправляет ACK-пакет с номером блока для подтверждения получения данных.
     *
     * @param block_number Номер блока данных. Тип: uint32_t.
     * @param sender_ep Точка назначения (IP-адрес и порт отправителя). Тип: boost::asio::ip::udp::endpoint.
     */
    void send_ack(uint32_t block_number, const boost::asio::ip::udp::endpoint &sender_ep);

    boost::asio::io_context &ioc_;              // Контекст ввода-вывода для Boost.Asio
    boost::asio::ip::udp::socket socket_;       // UDP-сокет для приема данных
    boost::asio::ip::udp::endpoint sender_ep_;  // Точка назначения для подтверждений
    std::string save_path_;             // Путь для сохранения полученного файла
    std::ofstream ofs_;                 // Поток для записи данных в файл
    uint32_t expected_block_ = 0;       // Ожидаемый номер следующего блока данных
    std::array<uint8_t, 2048> buffer_;  // Буфер для хранения получаемых данных
};

#endif