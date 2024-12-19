#define BOOST_TEST_MODULE MessengerTests
#include <boost/test/included/unit_test.hpp>
#include <thread>
#include <chrono>
#include "encryption.hpp"
#include "tcp_server.hpp"
#include "tcp_client.hpp"
#include "../include/database.hpp"
#include "udp_file_receiver.hpp"
#include "udp_file_sender.hpp"
#include "udp_file_server.hpp"
#include <boost/asio.hpp>
using boost::asio::ip::udp;


using namespace std;
// Тест шифрования
BOOST_AUTO_TEST_CASE(encryption_test) {
    string input = "Hello, Dear user!";
    string key = "HZ6S.D?e6S.D?6SS.D?e6S.?e6S";
    string encrypted = xor_encrypt(input, key);
    BOOST_CHECK_EQUAL(xor_decrypt(encrypted, key), input);

    // Сообщение об успешной проверке
    cout << "\033[32mSUCCESS: Encryption and decryption successful\033[0m" << endl;
}

// Тест бд
BOOST_AUTO_TEST_CASE(database_test) {
    Database db(":memory:");
    db.init();
    BOOST_CHECK(db.add_user("test_user", "password", "127.0.0.1", 8080));
    BOOST_CHECK(db.authenticate_and_update("test_user", "password", "127.0.0.1", 8081));
    BOOST_CHECK(!db.authenticate_and_update("test_user", "wrong_password", "127.0.0.1", 8081));

    // Сообщение об успешной проверке
    cout << "\033[32mSUCCESS: Database operations successful\033[0m" << endl;
}



// BOOST_AUTO_TEST_CASE(server_client_test) {
//     const uint16_t port = 5000;
//     boost::asio::io_context io_context;

//     // Запуск сервера в отдельном потоке
//     thread server_thread([&]() {
//         TCPServer server(io_context, port, ":memory:");
//         server.start();
//     });

//     // Увеличиваем задержку для запуска сервера
//     this_thread::sleep_for(chrono::seconds(3));

//     // Подключение клиента
//     try {
//         TCPClient client("127.0.0.1", port);
//         client.connect(); // Явно вызываем connect

//         // Успешное подключение клиента
//         cout << "\033[32mSUCCESS: Client connected to server\033[0m" << endl;

//         // Отправка сообщения
//         Message msg;
//         msg.type = MessageType::Text;
//         msg.text = "Hello, Server!";
//         client.send_message(msg);

//         // Получение ответа
//         Message response = client.receive_message();
//         BOOST_CHECK_EQUAL(response.text, "Hello, Client!");

//         // Сообщение об успешной обработке
//         cout << "\033[32mSUCCESS: Client-Server communication successful\033[0m" << endl;
//     } catch (const std::exception &e) {
//         BOOST_FAIL("Client or server error: " << e.what());
//     }

//     // Завершаем сервер
//     io_context.stop();
//     server_thread.join();
// }


// Тест UDP-соединения
BOOST_AUTO_TEST_CASE(server_client_udp_test) {
    const uint16_t port = 5000;
    boost::asio::io_context io_context;

    // Запуск UDP-сервера в отдельном потоке
    thread server_thread([&]() {
        // Инициализация UDP-сервера
        UDPFileServer server(io_context, udp::endpoint(udp::v4(), port));
        server.start();
    });

    // Увеличиваем задержку для запуска сервера
    this_thread::sleep_for(chrono::seconds(1));

    // Подключение UDP-клиента
    try {
        // Инициализация UDP-клиента
        udp::endpoint server_endpoint(boost::asio::ip::address::from_string("127.0.0.1"), port);
        UDPFileSender client(io_context, server_endpoint);

        // Успешное подключение клиента
        cout << "\033[32mSUCCESS: Client connected to UDP server\033[0m" << endl;

        // Отправка сообщения
        string message = "Hello, Server!";
        client.send_message(message);

        // Получение ответа
        string response = client.receive_message();
        BOOST_CHECK_EQUAL(response, "Hello, Client!");

        // Сообщение об успешной обработке
        cout << "\033[32mSUCCESS: Client-Server communication successful over UDP\033[0m" << endl;
    } catch (const std::exception &e) {
        BOOST_FAIL("Client or server error: " << e.what());
    }

    // Завершаем сервер
    io_context.stop();
    server_thread.join();
}