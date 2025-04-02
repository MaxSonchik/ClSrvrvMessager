#include "grpc_server.hpp"
#include <iostream>

namespace grpc_messenger {

grpc::Status ChatServiceImpl::ChatStream(grpc::ServerContext* context,
                                        grpc::ServerReaderWriter<ChatMessage, ChatMessage>* stream) {
    // First message from client should be CONNECT containing the username
    ChatMessage msg;
    std::string username;
    if (!stream->Read(&msg)) {
        return grpc::Status::OK;
    }
    if (msg.type() == ChatMessage::CONNECT) {
        username = msg.from();
        if (username.empty()) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Username required");
        }
        {
            std::lock_guard<std::mutex> lock(mtx_);
            clients_[username] = stream;
        }
        // Optionally, send a welcome message
        ChatMessage welcome;
        welcome.set_from("server");
        welcome.set_to(username);
        welcome.set_type(ChatMessage::TEXT);
        welcome.set_text("Connected");
        stream->Write(welcome);
    } else {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "First message must be CONNECT");
    }
    // Handle subsequent messages from this client
    ChatMessage incoming;
    while (stream->Read(&incoming)) {
        std::string from_user = username;
        if (incoming.type() == ChatMessage::TEXT) {
            std::string target = incoming.to();
            if (!target.empty()) {
                std::lock_guard<std::mutex> lock(mtx_);
                auto it = clients_.find(target);
                if (it != clients_.end()) {
                    ChatMessage outgoing = incoming;
                    outgoing.set_from(from_user);
                    it->second->Write(outgoing);
                } else {
                    // Target not online, send error back
                    ChatMessage error;
                    error.set_from("server");
                    error.set_to(from_user);
                    error.set_type(ChatMessage::TEXT);
                    error.set_text("User " + target + " not online");
                    stream->Write(error);
                }
            }
        } else if (incoming.type() == ChatMessage::FILE_START || incoming.type() == ChatMessage::FILE_CHUNK || incoming.type() == ChatMessage::FILE_END) {
            // Relay file transfer messages to target
            std::string target = incoming.to();
            if (!target.empty()) {
                std::lock_guard<std::mutex> lock(mtx_);
                auto it = clients_.find(target);
                if (it != clients_.end()) {
                    ChatMessage outgoing = incoming;
                    outgoing.set_from(from_user);
                    it->second->Write(outgoing);
                }
            }
        }
        // CONNECT messages beyond the first are ignored
    }
    // Client disconnected, remove from map
    {
        std::lock_guard<std::mutex> lock(mtx_);
        clients_.erase(username);
    }
    return grpc::Status::OK;
}

grpc::Status ChatServiceImpl::Ping(grpc::ServerContext* /*context*/,
                                   const Empty* /*request*/,
                                   Empty* /*response*/) {
    // Simply respond OK (empty response)
    return grpc::Status::OK;
}

grpc::Status ChatServiceImpl::UploadFile(grpc::ServerContext* /*context*/,
                                        grpc::ServerReader<FileChunk>* reader,
                                        Ack* response) {
    FileChunk chunk;
    size_t total_bytes = 0;
    while (reader->Read(&chunk)) {
        total_bytes += chunk.data().size();
        // Data can be written to file or processed as needed
    }
    response->set_success(true);
    response->set_message("Received " + std::to_string(total_bytes) + " bytes");
    return grpc::Status::OK;
}

} // namespace grpc_messenger
