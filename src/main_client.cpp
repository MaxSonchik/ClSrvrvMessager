#include "../include/common.hpp"
#include "../include/stun_client.hpp"
#include "../include/tcp_client.hpp"
#include "../include/udp_file_sender.hpp"
#include "../include/udp_file_receiver.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if(argc < 4) {
        std::cerr << "Usage: client <server_ip> <username> <mode>\n"
                  << "mode: send or receive\n";
        return 1;
    }

    std::string server_ip = argv[1];
    std::string username = argv[2];
    std::string mode = argv[3];

    StunClient stun(STUN_SERVER, STUN_PORT);
    PublicEndpoint pub_ep = stun.get_public_endpoint();

    TCPClient client(server_ip, SERVER_PORT);
    client.connect();
    client.register_client(username, pub_ep);

    if (mode=="send") {
        // Отправим текстовое сообщение
        Message msg;
        msg.type = MessageType::Text;
        msg.sender = username;
        msg.receiver = "another_user";
        msg.text = "Hello from NATed client!";
        client.send_message(msg);

        // Отправим файл (предположим, что another_user получил координаты)
        boost::asio::io_context ioc;
        UDPFileSender sender(ioc);
        sender.send_file("test.txt", "203.0.113.10", FILE_TRANSFER_PORT);

    } else if (mode=="receive") {
        // Запустим прием файла
        boost::asio::io_context ioc;
        UDPFileReceiver receiver(ioc, FILE_TRANSFER_PORT, "received_test.txt");
        receiver.start();
    }

    return 0;
}
