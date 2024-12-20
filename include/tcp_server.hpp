#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <unordered_map>

#include "common.hpp"
#include "database.hpp"
#include "message.hpp"

class TCPServer {
   public:
    /**
     * @brief Конструктор класса `TCPServer`.
     *
     * Инициализирует сервер с контекстом ввода-вывода, портом для прослушивания
     * и путем к базе данных для работы с клиентскими данными.
     *
     * @param ioc Контекст ввода-вывода для Boost.Asio. Тип: boost::asio::io_context.
     * @param port Порт для прослушивания входящих подключений. Тип: uint16_t.
     * @param db_path Путь к базе данных для работы с клиентами. Тип: std::string.
     */
    TCPServer(boost::asio::io_context &ioc, uint16_t port, const std::string &db_path);
    /**
     * @brief Запускает сервер и начинает прием подключений.
     *
     * Метод запускает сервер, начинает прослушивание порта и принимает подключения
     * от клиентов.
     */
    void start();

   private:
    /**
     * @brief Принимает входящие подключения от клиентов.
     *
     * Метод ожидает подключения клиентов, а затем вызывает обработчик для каждого
     * подключившегося клиента.
     *
     */
    void do_accept();
    /**
     * @brief Обрабатывает подключение клиента.
     *
     * Этот метод обрабатывает запросы от клиента, управляет его состоянием
     * и взаимодействует с базой данных.
     *
     * @param socket Умный указатель на сокет, через который осуществляется
     * соединение с клиентом. Тип: std::shared_ptr<boost::asio::ip::tcp::socket>.
     */
    void handle_client(std::shared_ptr<boost::asio::ip::tcp::socket> socket);

    boost::asio::io_context &ioc_;  // Контекст ввода-вывода для Boost.Asio.
    Database db_;  // Экземпляр базы данных для работы с клиентскими данными
    std::unordered_map<std::string, PublicEndpoint> clients_;  // Хранит известных клиентов с их публичными адресами
    std::unordered_map<std::string, std::shared_ptr<boost::asio::ip::tcp::socket>>
        sockets_;  // Хранит информацию о подключенных клиентах
    boost::asio::ip::tcp::acceptor acceptor_;  // Аксептор для прослушивания входящих подключений
};

#endif
