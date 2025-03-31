#include "grpc/grpc_server.hpp"
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <iostream>

int main() {
    grpc_messenger::ChatServiceImpl service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "gRPC Server started on port 50051\n";
    server->Wait();
    return 0;
}
