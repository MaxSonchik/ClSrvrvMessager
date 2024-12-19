#include "../include/tcp_server.hpp"
#include "../include/message.hpp"
#include <boost/asio.hpp>
#include <iostream>

using namespace boost::asio;
using tcp = ip::tcp;

TCPServer::TCPServer(boost::asio::io_context &ioc, uint16_t port, const std::string &db_path)
: ioc_(ioc)
, db_(db_path)
, acceptor_(ioc, tcp::endpoint(ip::make_address("0.0.0.0"), port))
{
    db_.init();
    log_info("Server created on IP 95.24.129.89 and port " + std::to_string(port));
}

void TCPServer::start() {
    log_info("Server started on port " + std::to_string(SERVER_PORT));
    do_accept();
    std::cout<<"Listening......"<<std::endl;
    ioc_.run();
}

void TCPServer::do_accept() {
    auto socket = std::make_shared<tcp::socket>(ioc_);
    acceptor_.async_accept(*socket, [this, socket](boost::system::error_code ec) {
        if (!ec) {
            log_info("New client connected");
            handle_client(socket);
        } else {
            log_error("Error accepting client: " + ec.message());
        }
        do_accept();
    });
}



static void async_read_msg(std::shared_ptr<tcp::socket> socket, std::function<void(Message)> handler) {
    auto length_buf = std::make_shared<std::vector<uint8_t>>(4);
    async_read(*socket, buffer(*length_buf), [socket, length_buf, handler](boost::system::error_code ec, std::size_t){
        if(ec) return;
        uint32_t msg_length = (uint32_t)((*length_buf)[0] | ((*length_buf)[1]<<8) | ((*length_buf)[2]<<16) | ((*length_buf)[3]<<24));

        auto msg_buf = std::make_shared<std::vector<uint8_t>>(msg_length);
        async_read(*socket, buffer(*msg_buf), [socket, msg_buf, handler](boost::system::error_code ec, std::size_t){
            if(ec) return;
            Message msg = deserialize_message(*msg_buf);
            handler(msg);
        });
    });
}

void TCPServer::handle_client(std::shared_ptr<tcp::socket> socket) {
    async_read_msg(socket, [this, socket](Message msg){
        if (msg.type == MessageType::ClientRegistration) {
            std::string username = msg.sender;
            std::string password = msg.password;
            std::string ip = msg.text;
            uint16_t port = (uint16_t)msg.file_size;

            bool user_ex = db_.user_exists(username);
            bool auth = db_.authenticate_and_update(username, password, ip, port);

            Message reply;
            reply.type = MessageType::Text;
            reply.sender = "server";
            reply.receiver = username;

            if (!user_ex && auth) {
                // новый пользователь
                reply.text = "Ok";
                reply.filename = "new";
                clients_[username] = {ip,port};
                sockets_[username] = socket;
            } else if (user_ex && auth) {
                // существующий пользователь + верный пароль
                reply.text = "Ok";
                reply.filename = "exists";
                clients_[username] = {ip, port};
                sockets_[username] = socket;
            } else {
                // неверный пароль
                reply.text = "Error";
            }

            std::vector<uint8_t> out = serialize_message(reply);
            async_write(*socket, buffer(out), [this,socket](auto,auto){});
            // После ответа, продолжаем читать новые сообщения
            handle_client(socket);
        } else if (msg.type == MessageType::UserStatusRequest) {
            Message status_reply;
            status_reply.type = MessageType::Text;
            status_reply.sender = "server";
            status_reply.receiver = msg.sender;
            // Проверяем онлайн ли msg.receiver
            if (sockets_.find(msg.receiver) != sockets_.end()) {
                status_reply.text = "online";
            } else {
                status_reply.text = "offline";
            }
            std::vector<uint8_t> out = serialize_message(status_reply);
            async_write(*socket, buffer(out), [this,socket](auto,auto){});
            handle_client(socket);
        } else if (msg.type == MessageType::Text) {
            // Пересылаем сообщение
            auto it = sockets_.find(msg.receiver);
            if (it!=sockets_.end()) {
                std::vector<uint8_t> out = serialize_message(msg);
                async_write(*(it->second), buffer(out), [](auto,auto){});
            } else {
                std::cout<<"ATTENTION: User is offline"<<std::endl;
            }
            handle_client(socket);
        }
    });
}

