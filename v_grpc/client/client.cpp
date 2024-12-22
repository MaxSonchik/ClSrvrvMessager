#include <iostream>
#include <grpcpp/grpcpp.h>
#include "messenger.grpc.pb.h"
#include "messenger.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using messenger::Messenger;
using messenger::MessageRequest;
using messenger::MessageResponse;
using messenger::Empty;

class MessengerClient {
public:
    MessengerClient(std::shared_ptr<Channel> channel)
        : stub_(Messenger::NewStub(channel)) {}

    void SendMessage(const std::string& user, const std::string& message) {
        MessageRequest request;
        request.set_user(user);
        request.set_message(message);

        MessageResponse response;
        ClientContext context;

        Status status = stub_->SendMessage(&context, request, &response);
        if (status.ok()) {
            std::cout << "Message sent successfully!\n";
        } else {
            std::cerr << "Error sending message: " << status.error_message() << "\n";
        }
    }

private:
    std::unique_ptr<Messenger::Stub> stub_;
};

int main(int argc, char** argv) {
    MessengerClient client(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));

    std::string user = "User1";
    std::string message;
    std::cout << "Enter your message: ";
    std::getline(std::cin, message);

    client.SendMessage(user, message);

    return 0;
}
