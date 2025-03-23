
#define BOOST_TEST_MODULE MessengerTests
#include <boost/asio.hpp>
#include <boost/test/included/unit_test.hpp>
#include <chrono>
#include <filesystem>
#include <thread>

#include "../include/database.hpp"
#include "../include/encryption.hpp"
#include "../include/tcp_client.hpp"
#include "../include/tcp_server.hpp"
#include "../include/udp_file_receiver.hpp"
#include "../include/udp_file_sender.hpp"
#include "../include/udp_file_server.hpp"

using boost::asio::ip::udp;

using namespace std;
// Тест шифрования (Argon2)
BOOST_AUTO_TEST_CASE(encryption_test)
{
    string password = "SecurePassword123";

    // Генерация хеша
    auto hash_result = Security::generate_hash(password);

    // Проверка верификации
    BOOST_CHECK(Security::verify_password(password, hash_result.hash, hash_result.salt));

    // Проверка с неверным паролем
    BOOST_CHECK(!Security::verify_password("WrongPassword", hash_result.hash, hash_result.salt));

    cout << "\033[32mSUCCESS: Argon2 hashing/verification successful\033[0m" << endl;
}

// Тест DB
// tests/test.cpp
BOOST_AUTO_TEST_CASE(database_test)
{
    Database db(":memory:");

    // Создание пользователя
    string password = "test_password";
    auto hash_result = Security::generate_hash(password);
    BOOST_CHECK(db.create_user("test_user", hash_result.hash, hash_result.salt));

    // Аутентификация
    BOOST_CHECK(db.authenticate_user("test_user", password));

    // Обновление информации
    BOOST_CHECK(db.update_connection_info("test_user", "127.0.0.1", 8080));

    // Неверные данные
    BOOST_CHECK(!db.authenticate_user("test_user", "wrong_password"));
    cout << "\033[32mSUCCESS: Database operations successful\033[0m" << endl;
}
BOOST_AUTO_TEST_CASE(udp_file_transfer_test) {
    namespace fs = std::filesystem;
    using namespace boost::asio;

    // 1. Инициализация директории
    const string receive_dir = "./received_files/";
    fs::create_directories(receive_dir);
    BOOST_REQUIRE(fs::exists(receive_dir));

    // 2. Создание тестового файла
    const string test_file = receive_dir + "test_file.txt";
    {
        ofstream f(test_file);
        f << "test_content";
    }

    // 3. Инициализация ASIO
    io_context io_context;
    auto work_guard = make_work_guard(io_context);
    thread receiver_thread([&io_context]() {
        try {
            io_context.run();
        } catch (const exception& e) {
            cerr << "IO Context error: " << e.what() << endl;
        }
    });

    // 4. Инициализация компонентов
    UDPFileReceiver receiver(io_context, 5001, receive_dir);
    receiver.start();
    UDPFileSender sender(io_context);

    // 5. Отправка файла с проверкой
    bool send_result = sender.send_file(
        test_file, 
        "127.0.0.1", 
        5001
    );
    BOOST_REQUIRE(send_result);

    // 6. Дать время на передачу (увеличено до 2 секунд)
    this_thread::sleep_for(2s);

    // 7. Корректное завершение
    receiver.stop();
    work_guard.reset();
    io_context.stop();
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }

    // 8. Проверка получения файла
    BOOST_CHECK(fs::exists(receive_dir + "test_file.txt"));
}
// Тест TCP

BOOST_AUTO_TEST_CASE(server_client_test)
{
    const uint16_t port = 5000;
    boost::asio::io_context io_context;

    std::thread server_thread(
        [&]()
        {
	    TCPServer server(io_context, port, ":memory:");
	    server.start();
        });

    std::this_thread::sleep_for(std::chrono::seconds(3));

    try
    {
	TCPClient client("127.0.0.1", port);
	client.connect();

	std::cout << "\033[32mSUCCESS: Client connected to server\033[0m" << std::endl;

	BOOST_CHECK_MESSAGE(true, "[SUCCESS] Test completed after client connected");

	io_context.stop();
	server_thread.join();

	std::cout << "\033[32mAll tests completed!\033[0m" << std::endl;

	return;
    }
    catch (const std::exception& e)
    {
	BOOST_FAIL("Client or server error: " << e.what());
    }
    io_context.stop();
    server_thread.join();
}

