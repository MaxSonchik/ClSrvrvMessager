#include "../include/tcp_server.hpp"
#include "../include/message.hpp"
#include <boost/asio.hpp>

using namespace boost::asio;
using tcp = ip::tcp;

TCPServer::TCPServer(boost::asio::io_context &ioc, uint16_t port, const std::string &db_path)
: ioc_(ioc),
  acceptor_(ioc, tcp::endpoint(tcp::v4(), port)),
  db_(db_path)
{
    db_.init(); // Создаёт таблицу, если её нет
}


void TCPServer::start() {
    log_info("Server started on port " + std::to_string(SERVER_PORT));
    do_accept();
    ioc_.run();
}

void TCPServer::do_accept() {
    auto socket = std::make_shared<tcp::socket>(ioc_);
    acceptor_.async_accept(*socket, [this, socket](boost::system::error_code ec){
        if (!ec) {
            log_info("New client connected");
            handle_client(socket);
        }
        do_accept();
    });
}

void TCPServer::handle_client(std::shared_ptr<tcp::socket> socket) {
    // Читаем сначала 4 байта длины
    auto length_buf = std::make_shared<std::vector<uint8_t>>(4);
    async_read(*socket, buffer(*length_buf), [this, socket, length_buf](boost::system::error_code ec, std::size_t){
        if(ec) return;
        uint32_t msg_length = (uint32_t)((*length_buf)[0] | ((*length_buf)[1]<<8) | ((*length_buf)[2]<<16) | ((*length_buf)[3]<<24));

        auto msg_buf = std::make_shared<std::vector<uint8_t>>(msg_length);
        async_read(*socket, buffer(*msg_buf), [this, socket, msg_buf](boost::system::error_code ec, std::size_t){
            if(ec) return;
            Message msg = deserialize_message(*msg_buf);
            if (msg.type == MessageType::ClientRegistration) {
                clients_[msg.sender] = {msg.text, (uint16_t)msg.file_size};
                log_info("Registered client: " + msg.sender + " => " + msg.text + ":" + std::to_string((uint16_t)msg.file_size));
            } else if (msg.type == MessageType::Text) {
                // Пересылаем получателю, если он онлайн
                auto it = clients_.find(msg.receiver);
                if (it!=clients_.end()) {
                    // В реальности нужно хранить socket клиента, а не только endpoint.
                    // Или реализовать сервер как роутер сообщений.
                    // Для простоты предполагаем, что все сообщения пересылаются тому же socket (в реальности - нужно адресовать конкретному клиенту)
                    // Здесь мы просто эхо-сервер:
                    std::vector<uint8_t> out = serialize_message(msg);
                    async_write(*socket, buffer(out), [](boost::system::error_code, std::size_t){});
                }
            }
            // Снова ждем сообщений
            handle_client(socket);
        });
    });
}
