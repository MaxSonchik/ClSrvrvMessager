#define BOOST_TEST_MODULE MessengerTests
#include <boost/test/included/unit_test.hpp>
#include <thread>
#include <chrono>
#include "encryption.hpp"
#include "tcp_server.hpp"
#include "tcp_client.hpp"
#include "database.hpp"

// Тест шифрования
BOOST_AUTO_TEST_CASE(encryption_test) {
    std::string input = "Hello, World!";
    std::string key = "key";
    std::string encrypted = xor_encrypt(input, key);
    BOOST_CHECK_EQUAL(xor_decrypt(encrypted, key), input);

    // Сообщение об успешной проверке
    std::cout << "\033[32mSUCCESS: Encryption and decryption successful\033[0m" << std::endl;
}

// тест бд
BOOST_AUTO_TEST_CASE(database_test) {
    Database db(":memory:");
    db.init();
    BOOST_CHECK(db.add_user("test_user", "password", "127.0.0.1", 8080));
    BOOST_CHECK(db.authenticate_and_update("test_user", "password", "127.0.0.1", 8081));
    BOOST_CHECK(!db.authenticate_and_update("test_user", "wrong_password", "127.0.0.1", 8081));

    // Сообщение об успешной проверке
    std::cout << "\033[32mSUCCESS: Database operations successful\033[0m" << std::endl;
}


BOOST_AUTO_TEST_CASE(server_client_test) {
    const uint16_t port = 5000;
    boost::asio::io_context io_context;

    // Запуск сервера в отдельном потоке
    std::thread server_thread([&]() {
        TCPServer server(io_context, port, ":memory:");
        server.start();
    });

    // Увеличиваем задержку для запуска сервера
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Подключение клиента
    try {
        TCPClient client("127.0.0.1", port);
        client.connect(); // Явно вызываем connect

        // Успешное подключение клиента
        std::cout << "\033[32mSUCCESS: Client connected to server\033[0m" << std::endl;

        // Отправка сообщения
        Message msg;
        msg.type = MessageType::Text;
        msg.text = "Hello, Server!";
        client.send_message(msg);

        // Получение ответа
        Message response = client.receive_message();
        BOOST_CHECK_EQUAL(response.text, "Hello, Client!");

        // Сообщение об успешной обработке
        std::cout << "\033[32mSUCCESS: Client-Server communication successful\033[0m" << std::endl;
    } catch (const std::exception &e) {
        BOOST_FAIL("Client or server error: " << e.what());
    }

    // Завершаем сервер
    io_context.stop();
    server_thread.join();
}
