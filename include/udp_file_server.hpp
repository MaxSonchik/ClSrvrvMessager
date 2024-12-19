#include "../include/file_transfer_protocol.hpp"
#include <boost/asio.hpp>
#include <fstream>
#include <iostream>
/**
 * @class UDPFileServer
 * @brief Класс для приема файлов через UDP.
 *
 * Класс предоставляет функциональность для получения файлов, разбивая их на блоки,
 * сохранения данных в файл, а также отправки подтверждений (ACK) для каждого блока.
 * 
 * `public`:
 * 
 * Конструктор класса `UDPFileServer`.
 * 
 *  Инициализирует сервер для приема UDP-пакетов. Создает сокет для приема данных с указанного порта.
 *
 *  @param ioc Контекст ввода-вывода для Boost.Asio. Тип: boost::asio::io_context&.
 *  @param port Порт, на котором сервер будет ожидать пакеты. Тип: uint16_t.
 */
class UDPFileServer {
public:
    UDPFileServer(boost::asio::io_context &ioc, uint16_t port)
        : socket_(ioc, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)) {
        start_receive();
    }

private:
    boost::asio::ip::udp::socket socket_; //UDP-сокет для приема данных
    std::vector<uint8_t> buffer_; //Буфер для хранения полученных данных
    std::ofstream outfile_;
    /**
     * @brief Запускает асинхронный прием пакетов.
     *
     * Метод запускает асинхронный прием пакетов через сокет. Когда пакет получен,
     * вызывается обработчик пакета. После обработки снова вызывается прием.
     * - buffer_.resize() - Размер буфера
     * 
     */
    void start_receive() {
        buffer_.resize(1024);
        socket_.async_receive_from(boost::asio::buffer(buffer_), remote_endpoint_,
            [this](boost::system::error_code ec, std::size_t bytes_received) {
                if (!ec && bytes_received > 0) {
                    handle_packet(buffer_, bytes_received);
                }
                start_receive();
            });
    }
    /**
     * @brief Обрабатывает полученный пакет.
     *
     * Данный метод десериализует данные в структуру пакета, записывает данные
     * в файл и отправляет подтверждение (ACK) о получении блока.
     *
     * @param buf Буфер с данными, полученными от отправителя. Тип: std::vector<uint8_t>.
     * @param size Размер полученных данных. Тип: std::size_t.
     */
    void handle_packet(const std::vector<uint8_t> &buf, [[maybe_unused]]std::size_t size) {
        FileDataPacket packet;
        if (parse_data_packet(buf, packet)) {
            // Открываем файл, если первый блок
            if (packet.block_number == 0) {
                outfile_.open("received_file", std::ios::binary);
            }

            // Пишем данные в файл
            outfile_.write(reinterpret_cast<const char *>(packet.data.data()), packet.data.size());

            // Отправляем ACK
            FileAckPacket ack{packet.block_number};
            auto ack_buf = serialize_ack_packet(ack);
            socket_.send_to(boost::asio::buffer(ack_buf), remote_endpoint_);

            // Закрываем файл, если последний блок
            if (packet.data.empty()) {
                outfile_.close();
                std::cout << "File received successfully.\n";
            }
        }
    }

    boost::asio::ip::udp::endpoint remote_endpoint_;
};
