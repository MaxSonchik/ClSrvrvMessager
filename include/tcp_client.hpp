#ifndef TCP_CLIENT_HPP
#define TCP_CLIENT_HPP
#include <string>
#include <boost/asio.hpp>
#include "message.hpp"
#include "common.hpp"
/**
 * @class TCPClient
 * @brief Класс для работы с сервером через протокол TCP.
 *
 *   Класс предоставляет функциональность для:
 * 
 * - Подключения к серверу.
 * 
 * - Регистрации клиента на сервере.
 * 
 * - Отправки сообщений на сервер.
 * 
 * - Получения сообщений от сервера.
 */
class TCPClient {
public:
    /**
     * @brief Конструктор класса `TCPClient`.
     *
     * Инициализирует TCP-клиент с адресом и портом сервера.
     *
     * @param server_ip IP-адрес сервера. Тип: std::string.
     * @param server_port Порт сервера. Тип: uint16_t.
     */
    TCPClient(const std::string &server_ip, uint16_t server_port);
    
    /**
     * @brief Деструктор класса TCPClient

     */
    ~TCPClient(); 

    /**
     * @brief Устанавливает подключение к серверу.
     *
     * Использует предоставленные адрес и порт для установления TCP-соединения.
     *
     * @throws boost::system::system_error Если не удается подключиться к серверу.
     */
    void connect();
    /**
     * @brief Отправляет сообщение на сервер.
     *
     * Сериализует структуру `Message` и отправляет её на сервер через
     * установленное TCP-соединение.
     *
     * @param msg Сообщение для отправки. Тип: Message.
     * 
     */
    void send_message(const Message &msg);
    /**
     * @brief Регистрирует клиента на сервере.
     *
     * Отправляет на сервер данные для регистрации, такие как имя пользователя
     * и пароль.
     *
     * @param username Имя пользователя. Тип: std::string.
     * @param password Пароль пользователя. Тип: std::string.
     * 
     */
    void register_client(const std::string &username, const std::string &password);
    /**
     * @brief Получает сообщение от сервера.
     *
     * Считывает данные из сокета, десериализует их и возвращает структуру `Message`.
     *
     * @return `Message` Полученное сообщение.
     * 
     */
    Message receive_message();

private:
    /**
     * @brief Считывает сообщение из сокета.
     *
     * Этот метод используется для получения данных и преобразования их в объект `Message`.
     *
     * @return Message Десериализованное сообщение.
     */
    Message read_message();

    std::string server_ip_; // IP-адрес сервера
    uint16_t server_port_; //Порт сервера
    boost::asio::io_context ioc_; //Контекст ввода-вывода для работы с Boost.Asio
    boost::asio::ip::tcp::socket socket_; // Сокет для подключения к серверу
};
#endif
