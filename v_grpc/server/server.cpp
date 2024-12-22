#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "messenger.grpc.pb.h"
#include "messenger.pb.h"


using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using messenger::Messenger;
using messenger::MessageRequest;
using messenger::MessageResponse;
using messenger::Empty;
using messenger::Message;

class MessengerServiceImpl final : public Messenger::Service {
    Status SendMessage(ServerContext* context, const MessageRequest* request, MessageResponse* response) override {
        std::cout << "Message from " << request->user() << ": " << request->message() << "\n";
        response->set_success(true);
        return Status::OK;
    }

    Status ReceiveMessages(ServerContext* context, const Empty* request, grpc::ServerWriter<Message>* writer) override {
        // Стриминг сообщений
        return Status::OK;
    }
};

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    MessengerServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << "\n";
    server->Wait();
}

int main() {
    RunServer();
    return 0;
}
