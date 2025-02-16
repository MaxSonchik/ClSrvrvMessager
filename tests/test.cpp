
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
    db.init();

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
    db.~Database();
    cout << "\033[32mSUCCESS: Database operations successful\033[0m" << endl;
}

BOOST_AUTO_TEST_CASE(udp_file_transfer_test)
{
    // ----- Параметры теста -----
    const std::string save_path = "./received_files/";
    const std::string test_file = "./test_data/test_file.txt";
    const short port = 54321 + (std::hash<std::thread::id>{}(std::this_thread::get_id()) % 100);

    // 1. Инициализация контекста и work guard
    boost::asio::io_context io_context;
    auto work_guard = boost::asio::make_work_guard(io_context);

    // 2. Создаём receiver в общей области видимости
    UDPFileReceiver* receiver_ptr = nullptr;
    std::promise<void> receiver_ready;
    std::future<void> ready_future = receiver_ready.get_future();

    // 3. Запускаем поток приёмника
    std::thread receiver_thread(
        [&]()
        {
	    UDPFileReceiver receiver(io_context, port, save_path);
	    receiver_ptr = &receiver; // Сохраняем указатель
	    receiver_ready.set_value();
	    receiver.start();
        });

    // 4. Ожидание инициализации приёмника
    ready_future.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 5. Отправка файла
    bool success = false;
    try
    {
	UDPFileSender sender(io_context);
	success = sender.send_file(test_file, "127.0.0.1", port);
	BOOST_CHECK(success);
    }
    catch (...)
    {
	BOOST_FAIL("Exception during file sending");
    }

    // 6. Остановка
    work_guard.reset(); // Разрешаем завершение io_context
    io_context.stop();  // Останавливаем контекст

    // 7. Остановка приёмника через указатель
    if (receiver_ptr)
    {
	receiver_ptr->stop();
    }

    // 8. Ожидание завершения потока
    if (receiver_thread.joinable())
    {
	receiver_thread.join();
    }

    // 9. Проверка результата
    bool file_found = false;
    try
    {
	// Проверяем содержимое директории save_path
	for (const auto& entry : std::filesystem::directory_iterator(save_path))
	{
	    if (entry.path().extension() == ".bin")
	    {
		file_found = true;

		// Проверяем размер файла
		auto file_size = std::filesystem::file_size(entry.path());
		BOOST_CHECK_MESSAGE(file_size > 0,
		                    "Received file is empty: " + entry.path().string());

		// Логируем успешное получение файла
		std::cout << "[SUCCESS] File received: " << entry.path().string()
		          << " (size: " << file_size << " bytes)" << std::endl;

		// Дополнительная проверка содержимого (опционально)
		std::ifstream received_file(entry.path(), std::ios::binary);
		if (received_file)
		{
		    std::string content((std::istreambuf_iterator<char>(received_file)),
		                        std::istreambuf_iterator<char>());
		    BOOST_CHECK_MESSAGE(!content.empty(), "Received file content is empty");
		}
		break; // Нашли файл, дальше проверять не нужно
	    }
	}

	// Если файл не найден
	BOOST_CHECK_MESSAGE(file_found, "No received file found in directory: " + save_path);

	// Очистка директории после теста (опционально)
	// for (const auto& entry : std::filesystem::directory_iterator(save_path)) {
	//     std::filesystem::remove(entry.path());
	// }
    }
    catch (const std::exception& e)
    {
	BOOST_FAIL("Exception during file verification: " + std::string(e.what()));
    }
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
