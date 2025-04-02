#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <atomic>
#include <boost/asio.hpp>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

namespace tcp_messenger {

class TCPClient {
public:
  TCPClient(const std::string &username, const std::string &server_host,
            unsigned short server_port);
  ~TCPClient();
  // Runs the client: connects to server and enters command loop
  void run();
  // Send a text message to a target user
  void sendMessage(const std::string &to, const std::string &text);
  // Send a file to a target user (via UDP with ACK)
  void sendFile(const std::string &to, const std::string &file_path);
  // Disconnect from server
  void disconnect();

private:
  void start_read_tcp();
  void start_receive_udp();
  void handle_server_message(const std::string &message);

  std::string username_;
  std::string server_host_;
  unsigned short server_port_;
  boost::asio::io_context io_context_;
  boost::asio::ip::tcp::socket tcp_socket_;
  boost::asio::ip::udp::socket udp_socket_;
  boost::asio::ip::udp::endpoint sender_endpoint_;
  boost::asio::streambuf tcp_buf_;
  std::thread io_thread_;
  std::atomic<bool> running_;
  // Synchronization for file transfer handshake (file_ready)
  std::atomic<bool> waiting_file_ready_;
  std::mutex file_ready_mutex_;
  std::condition_variable file_ready_cv_;
  // Synchronization for ACK reception
  std::mutex ack_mutex_;
  std::condition_variable ack_cv_;
  int expected_ack_id_;
  bool ack_received_;
  // File transfer state
  std::string pending_file_name_;
  std::string pending_file_target_;
  std::string file_recv_name_;
  std::ofstream file_recv_stream_;
  int last_received_chunk_id_;
  // Receiver's network info for pending file (set from server's file_ready)
  std::string receiver_ip_;
  unsigned short receiver_port_;
};

} // namespace tcp_messenger

#endif
