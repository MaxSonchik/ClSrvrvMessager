#include "tcp_server.hpp"
#include "common/json_util.h"
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

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
                if (ec) server_.remove_client(username_);
            });
    }

    std::string get_username() const { return username_; }
    void set_username(const std::string& name) { username_ = name; }
    tcp::endpoint remote_endpoint() const { return socket_.remote_endpoint(); }

private:
    void do_read() {
        auto self = shared_from_this();
        boost::asio::async_read_until(socket_, buffer_, '\n',
            [this, self](boost::system::error_code ec, std::size_t /*bytes*/) {
                if (!ec) {
                    std::string line;
                    std::istream is(&buffer_);
                    std::getline(is, line);
                    if (!line.empty()) server_.handle_message(line, self);
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
      acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
      exposer_("0.0.0.0:9090"),
      registry_(std::make_shared<prometheus::Registry>()),
      messages_total_(prometheus::BuildCounter()
          .Name("messages_total")
          .Help("Total processed messages")
          .Register(*registry_)
          .Add({})),
      active_connections_(prometheus::BuildGauge()
          .Name("active_connections")
          .Help("Current active connections")
          .Register(*registry_)
          .Add({})) {
    exposer_.RegisterCollectable(registry_);
}

void TCPServer::start() { do_accept(); }

void TCPServer::do_accept() {
    acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec) {
            active_connections_.Increment();
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

    const std::string event = data.value("event", "");
    const std::string from = session->get_username();

    if (event == "connect") {
        const std::string user = data.value("from", "");
        if (!user.empty()) {
            session->set_username(user);
            std::lock_guard<std::mutex> lock(clients_mutex_);
            clients_[user] = session;
            std::cout << common::make_json_event("connect", user) << std::endl;
        }
        return;
    }

    if (event == "message") {
        const std::string to = data.value("to", "");
        const std::string text = data.value("text", "");
        
        if (!to.empty() && !text.empty()) {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            if (auto it = clients_.find(to); it != clients_.end()) {
                it->second->send(common::make_json_event("message", from, to, text) + "\n");
            } else if (auto sender = clients_.find(from); sender != clients_.end()) {
                sender->second->send(common::make_json_event("error", "server", from, "User " + to + " offline") + "\n");
            }
            messages_total_.Increment();
        }
    }
}

void TCPServer::remove_client(const std::string& username) {
    if (!username.empty()) {
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            clients_.erase(username);
        }
        active_connections_.Decrement();
        std::cout << common::make_json_event("disconnect", username) << std::endl;
    }
}

void TCPServer::stop() {
    boost::system::error_code ec;
    acceptor_.close(ec);
}

} // namespace tcp_messenger