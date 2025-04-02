#include "grpc_client.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <algorithm>

namespace grpc_messenger {

GrpcClient::GrpcClient(const std::string& username, const std::string& server_address)
    : username_(username), server_address_(server_address), connected_(false) {
    auto channel = grpc::CreateChannel(server_address_, grpc::InsecureChannelCredentials());
    stub_ = ChatService::NewStub(channel);
}

GrpcClient::~GrpcClient() {
    disconnect();
    if (reader_thr_.joinable()) {
        reader_thr_.join();
    }
}

void GrpcClient::run() {
    // Open a bi-directional stream to the server
    stream_ = stub_->ChatStream(&context_);
    // Send a CONNECT message to register username
    ChatMessage connect_msg;
    connect_msg.set_from(username_);
    connect_msg.set_type(ChatMessage::CONNECT);
    stream_->Write(connect_msg);
    connected_ = true;
    // Start a background thread to read incoming messages from server
    reader_thr_ = std::thread(&GrpcClient::readerThread, this);
    // Command input loop
    std::string input;
    while (connected_ && std::getline(std::cin, input)) {
        if (input.rfind("/msg ", 0) == 0) {
            size_t space = input.find(' ', 5);
            if (space != std::string::npos) {
                std::string target = input.substr(5, space - 5);
                std::string text = input.substr(space + 1);
                sendMessage(target, text);
            }
        } else if (input.rfind("/file ", 0) == 0) {
            size_t space = input.find(' ', 6);
            if (space != std::string::npos) {
                std::string target = input.substr(6, space - 6);
                std::string path = input.substr(space + 1);
                sendFile(target, path);
            }
        } else if (input == "/ping") {
            // Use Ping RPC for latency test
            Empty req, resp;
            grpc::ClientContext ping_ctx;
            grpc::Status status = stub_->Ping(&ping_ctx, req, &resp);
            if (status.ok()) {
                std::cout << "{\"event\":\"pong\"}" << std::endl;
            } else {
                std::cout << "{\"event\":\"error\",\"text\":\"Ping failed\"}" << std::endl;
            }
        } else if (input == "/exit") {
            break;
        } else {
            std::cerr << "Unknown command.\n";
        }
    }
    // Disconnect and end
    disconnect();
}

void GrpcClient::readerThread() {
    // Continuously read messages from the server stream
    ChatMessage msg;
    while (stream_->Read(&msg)) {
        if (msg.type() == ChatMessage::TEXT) {
            // Received a text message
            std::string from = msg.from();
            std::string text = msg.text();
            std::cout << "{\"event\":\"message\",\"from\":\"" << from << "\",\"text\":\"" << text << "\"}" << std::endl;
        } else if (msg.type() == ChatMessage::FILE_START) {
            // File transfer initiation from another user
            std::string from = msg.from();
            std::string file_name = msg.file_name();
            int total_chunks = msg.total_chunks();
            std::filesystem::create_directories("received_files");
            std::ofstream ofs("received_files/" + file_name, std::ios::binary);
            // Receive all chunks
            for (int i = 0; i < total_chunks; ++i) {
                ChatMessage chunk_msg;
                if (!stream_->Read(&chunk_msg)) break;
                if (chunk_msg.type() != ChatMessage::FILE_CHUNK) break;
                ofs.write(chunk_msg.file_chunk().data(), chunk_msg.file_chunk().size());
            }
            ofs.close();
            std::cout << "{\"event\":\"file_received\",\"from\":\"" << from << "\",\"file_name\":\"" << file_name << "\"}" << std::endl;
        }
        // FILE_CHUNK messages are handled in the loop above when a FILE_START is encountered
    }
    connected_ = false;
}

void GrpcClient::sendMessage(const std::string& to, const std::string& text) {
    if (!connected_) return;
    ChatMessage msg;
    msg.set_from(username_);
    msg.set_to(to);
    msg.set_type(ChatMessage::TEXT);
    msg.set_text(text);
    stream_->Write(msg);
}

void GrpcClient::sendFile(const std::string& to, const std::string& file_path) {
    if (!connected_) return;
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open file: " << file_path << std::endl;
        return;
    }
    // Determine file name and size
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0);
    std::string file_name = file_path;
    size_t pos = file_path.find_last_of("/\\");
    if (pos != std::string::npos) file_name = file_path.substr(pos + 1);
    // Calculate number of chunks
    size_t chunk_size = 1024;
    int total_chunks = (size + chunk_size - 1) / chunk_size;
    // Send FILE_START message
    ChatMessage start_msg;
    start_msg.set_from(username_);
    start_msg.set_to(to);
    start_msg.set_type(ChatMessage::FILE_START);
    start_msg.set_file_name(file_name);
    start_msg.set_file_size((int64_t)size);
    start_msg.set_total_chunks(total_chunks);
    stream_->Write(start_msg);
    // Send file chunks
    std::vector<char> buffer(chunk_size);
    int chunk_id = 0;
    auto start_time = std::chrono::steady_clock::now();
    while (file.good() && chunk_id < total_chunks) {
        file.read(buffer.data(), buffer.size());
        std::streamsize bytes_read = file.gcount();
        if (bytes_read <= 0) break;
        ChatMessage chunk_msg;
        chunk_msg.set_from(username_);
        chunk_msg.set_to(to);
        chunk_msg.set_type(ChatMessage::FILE_CHUNK);
        chunk_msg.set_chunk_id(chunk_id);
        chunk_msg.set_total_chunks(total_chunks);
        chunk_msg.set_file_chunk(std::string(buffer.data(), bytes_read));
        stream_->Write(chunk_msg);
        chunk_id++;
    }
    file.close();
    // (We rely on total_chunks instead of sending FILE_END)
    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    double throughput_mbps = (double)size * 8 / elapsed.count() / (1024.0 * 1024.0);
    std::cout << "{\"event\":\"file_sent\",\"to\":\"" << to << "\",\"file_name\":\"" << file_name << "\"}" << std::endl;
}

void GrpcClient::disconnect() {
    if (connected_) {
        connected_ = false;
        // Finish the stream
        stream_->WritesDone();
        stream_->Finish();
        context_.TryCancel();
    }
}

} // namespace grpc_messenger
