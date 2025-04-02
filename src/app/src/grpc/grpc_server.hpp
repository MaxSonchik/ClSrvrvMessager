#ifndef GRPC_SERVER_H
#define GRPC_SERVER_H

#include <grpcpp/grpcpp.h>
#include "chat.grpc.pb.h"
#include "chat.pb.h"

#include <string>
#include <unordered_map>
#include <mutex>


namespace grpc_messenger {

class ChatServiceImpl final : public ChatService::Service {
public:
    grpc::Status ChatStream(grpc::ServerContext* context,
                            grpc::ServerReaderWriter<ChatMessage, ChatMessage>* stream) override;
    grpc::Status Ping(grpc::ServerContext* context,
                      const Empty* request,
                      Empty* response) override;
    grpc::Status UploadFile(grpc::ServerContext* context,
                            grpc::ServerReader<FileChunk>* reader,
                            Ack* response) override;
private:
    std::mutex mtx_;
    // Map of username to active stream (for delivering messages)
    std::unordered_map<std::string, grpc::ServerReaderWriter<ChatMessage, ChatMessage>*> clients_;
};

} // namespace grpc_messenger

#endif
