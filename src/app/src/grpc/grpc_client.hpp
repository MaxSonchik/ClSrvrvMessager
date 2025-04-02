#ifndef GRPC_CLIENT_H
#define GRPC_CLIENT_H

#include "grpc/chat.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <thread>
#include <atomic>
#include <string>
#include <filesystem>

namespace grpc_messenger {

class GrpcClient {
public:
    GrpcClient(const std::string& username, const std::string& server_address);
    ~GrpcClient();
    void run();                          // Connects and enters input loop
    void sendMessage(const std::string& to, const std::string& text);
    void sendFile(const std::string& to, const std::string& file_path);
    void disconnect();

private:
    void readerThread();                 // Thread function to read incoming messages from stream
    std::string username_;
    std::string server_address_;
    std::unique_ptr<ChatService::Stub> stub_;
    grpc::ClientContext context_;
    std::shared_ptr<grpc::ClientReaderWriter<ChatMessage, ChatMessage>> stream_;
    std::thread reader_thr_;
    std::atomic<bool> connected_;
};

} // namespace grpc_messenger

#endif
