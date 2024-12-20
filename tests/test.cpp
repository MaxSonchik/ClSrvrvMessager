
#define BOOST_TEST_MODULE MessengerTests
#include <boost/asio.hpp>
#include <boost/test/included/unit_test.hpp>
#include <chrono>
#include <filesystem>
#include <thread>

#include "../include/database.hpp"
#include "encryption.hpp"
#include "tcp_client.hpp"
#include "tcp_server.hpp"
#include "udp_file_receiver.hpp"
#include "udp_file_sender.hpp"
#include "udp_file_server.hpp"

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

// Тест UDP
/*
BOOST_AUTO_TEST_CASE(udp_file_transfer_test) {
    const std::string save_path = "./received_files/";
    const std::string test_file = "./test_data/test_file.txt";
    const std::string dest_ip = "127.0.0.1";
    const short port = 12345;

    // Проверяем рабочую директорию
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        std::cout << "[DEBUG] Current working directory: " << cwd << std::endl;
    }

    // Удаляем старые файлы перед тестом
    try {
        for (const auto& entry : std::filesystem::directory_iterator(save_path)) {
            std::filesystem::remove(entry.path());
        }
        std::cout << "[INFO] Directory cleaned: " << save_path << std::endl;
    } catch (const std::exception& e) {
        BOOST_FAIL("[ERROR] Failed to clean directory: " + save_path + ", " + std::string(e.what()));
    }

    // Создаём тестовый файл, если его нет
    if (!std::filesystem::exists(test_file)) {
        try {
            std::ofstream test_file_stream(test_file);
            if (!test_file_stream) {
                BOOST_FAIL("[ERROR] Failed to create test file: " + test_file);
            }
            test_file_stream << "This is a test file for UDP transfer.";
            test_file_stream.close();
            std::cout << "[INFO] Test file created: " << test_file << std::endl;
        } catch (const std::exception& e) {
            BOOST_FAIL("[ERROR] Exception while creating test file: " + std::string(e.what()));
        }
    } else {
        std::cout << "[INFO] Test file already exists: " << test_file << std::endl;
    }

    // Создаём контекст Boost.Asio
    boost::asio::io_context io_context;

    // Запускаем UDPFileReceiver
    std::thread receiver_thread([&]() {
        UDPFileReceiver receiver(io_context, port, save_path);
        receiver.start();
    });

    // Даем время для запуска приёмника
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Отправляем файл
    try {
        UDPFileSender sender(io_context);
        bool success = sender.send_file(test_file, dest_ip, port);
        BOOST_CHECK_MESSAGE(success, "[ERROR] File sending failed.");
        std::cout << "\033[32mSUCCESS: Data sent successfully\033[0m" << std::endl;
    } catch (const std::exception& e) {
        BOOST_FAIL("[ERROR] Sender error: " + std::string(e.what()));
    }

    // Завершаем приёмник
    io_context.stop();
    receiver_thread.join();

    // Проверяем наличие полученного файла
    bool file_found = false;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(save_path)) {
            if (entry.path().extension() == ".bin") {
                file_found = true;
                std::cout << "\033[32mSUCCESS: File received successfully: " << entry.path() << "\033[0m" << std::endl;
            }
        }
        BOOST_CHECK_MESSAGE(file_found, "[ERROR] No received file found in " + save_path);
    } catch (const std::exception& e) {
        BOOST_FAIL("[ERROR] Exception while checking received file: " + std::string(e.what()));
    }
}
*/

// Тест TCP
/*
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
        client.connect();  // Явно вызываем connect

        // Успешное подключение клиента
        std::cout << "\033[32mSUCCESS: Client connected to server\033[0m" << std::endl;

        // Завершаем выполнение теста после успешного подключения клиента
        BOOST_CHECK_MESSAGE(true, "[SUCCESS] Test completed after client connected");

        // Останавливаем сервер
        io_context.stop();
        server_thread.join();

        // Выводим сообщение о завершении всех тестов
        std::cout << "\033[32mAll tests completed!\033[0m" << std::endl;

        return;  // Прерываем тест на этом этапе
    } catch (const std::exception& e) {
        BOOST_FAIL("Client or server error: " << e.what());
    }

    // Завершаем сервер, если тест не завершился раньше
    io_context.stop();
    server_thread.join();
}
*/
BOOST_AUTO_TEST_CASE(server_client_test) {
    const uint16_t port = 5000;
    boost::asio::io_context io_context;
    // Запуск сервера в отдельном потоке
    thread server_thread([&]() {
        TCPServer server(io_context, port, ":memory:");
        server.start();
    });
    // Увеличиваем задержку для запуска сервера
    this_thread::sleep_for(chrono::seconds(3));
    // Подключение клиента
    try {
        TCPClient client("127.0.0.1", port);
        client.connect();  // Явно вызываем connect
        // Успешное подключение клиента
        cout << "\033[32mSUCCESS: Client connected to server\033[0m" << endl;
        // Отправка сообщения
        Message msg;
        msg.type = MessageType::Text;
        msg.text = "Hello, Server!";
        client.send_message(msg);
        // Получение ответа
        Message response = client.receive_message();
        BOOST_CHECK_EQUAL(response.text, "Hello, Client!");
        // Сообщение об успешной обработке
        cout << "\033[32mSUCCESS: Client-Server communication successful\033[0m" << endl;
    } catch (const std::exception &e) {
        BOOST_FAIL("Client or server error: " << e.what());
    }
    // Завершаем сервер
    io_context.stop();
    server_thread.join();
}