#include "tcp_server.hpp"
#include "common/json_util.h"
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using boost::asio::ip::tcp;

namespace tcp_messenger {

class TCPServer::Session : public std::enable_shared_from_this<TCPServer::Session> {
public:
    Session(TCPServer& server, tcp::socket socket)
        : server_(server), socket_(std::move(socket)) {}

    void start() {
        do_read();
    }

    void send(const std::string& data) {
        auto self = shared_from_this();
        boost::asio::async_write(socket_, boost::asio::buffer(data),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (ec) {
                    server_.remove_client(username_);
                }
            });
    }

    std::string get_username() const {
        return username_;
    }

    void set_username(const std::string& name) {
        username_ = name;
    }

    tcp::endpoint remote_endpoint() const {
        return socket_.remote_endpoint();
    }

private:
    void do_read() {
        auto self = shared_from_this();
        boost::asio::async_read_until(socket_, buffer_, '\n',
            [this, self](boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
                if (!ec) {
                    std::string line;
                    std::istream is(&buffer_);
                    std::getline(is, line);
                    if (!line.empty()) {
                        server_.handle_message(line, self);
                    }
                    do_read();
                } else {
                    server_.remove_client(username_);
                }
            });
    }

    TCPServer& server_;
    tcp::socket socket_;
    boost::asio::streambuf buffer_;
    std::string username_;
};

TCPServer::TCPServer(boost::asio::io_context& io_context, unsigned short port)
    : io_context_(io_context),
      acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {}

void TCPServer::start() {
    do_accept();
}

void TCPServer::do_accept() {
    acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec) {
            auto session = std::make_shared<Session>(*this, std::move(socket));
            session->start();
        }
        do_accept();
    });
}

void TCPServer::handle_message(const std::string& message, std::shared_ptr<Session> session) {
    json data;
    try {
        data = json::parse(message);
    } catch (...) {
        std::cerr << "[SERVER] Invalid JSON: " << message << std::endl;
        return;
    }

    std::string event = data.value("event", "");
    std::string from = session->get_username();

    if (event == "connect") {
        std::string user = data.value("from", "");
        if (user.empty()) return;
        session->set_username(user);
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            clients_[user] = session;
        }
        std::cout << common::make_json_event("connect", user) << std::endl;
        return;
    }

    if (event == "message") {
        std::string to = data.value("to", "");
        std::string text = data.value("text", "");
        if (to.empty() || text.empty()) return;

        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(to);
        if (it != clients_.end()) {
            std::string json = common::make_json_event("message", from, to, text) + "\n";
            it->second->send(json);
        } else {
            auto it_sender = clients_.find(from);
            if (it_sender != clients_.end()) {
                std::string err = common::make_json_event("error", "server", from, "User " + to + " not online") + "\n";
                it_sender->second->send(err);
            }
        }
        return;
    }
}

void TCPServer::remove_client(const std::string& username) {
    if (username.empty()) return;
    std::lock_guard<std::mutex> lock(clients_mutex_);
    clients_.erase(username);
    udp_ports_.erase(username);
    std::string logMsg = common::make_json_event("disconnect", username);
    std::cout << logMsg << std::endl;
}

void TCPServer::stop() {
    boost::system::error_code ec;
    acceptor_.close(ec);
}

} // namespace tcp_messenger