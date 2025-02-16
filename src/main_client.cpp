#include <iostream>
#include <string>

#include "../include/common.hpp"
#include "../include/tcp_client.hpp"
#include "../include/udp_file_sender.hpp"

using namespace boost::asio;
using tcp = ip::tcp;

// Для удобства зададим какой-то дефолтный пароль (в реальном приложении храните/спрашивайте
// безопасно)
static const std::string DEFAULT_PASSWORD = "pass123";

// Переработанная функция отправки
bool send_message(tcp::socket& socket, const Message& msg, boost::system::error_code& ec)
{
    auto data = serialize_message(msg);

    // Отправляем длину сообщения
    uint32_t length = static_cast<uint32_t>(data.size());
    std::array<uint8_t, 4> length_buf{};
    length_buf[0] = static_cast<uint8_t>(length);
    length_buf[1] = static_cast<uint8_t>(length >> 8);
    length_buf[2] = static_cast<uint8_t>(length >> 16);
    length_buf[3] = static_cast<uint8_t>(length >> 24);

    write(socket, buffer(length_buf), ec);
    if (ec)
	return false;

    // Отправляем данные
    write(socket, buffer(data), ec);
    return !ec;
}

// Переработанная функция чтения
bool read_message(tcp::socket& socket, Message& out_msg, boost::system::error_code& ec)
{
    std::array<uint8_t, 4> length_buf{};
    read(socket, buffer(length_buf), ec);
    if (ec)
	return false;

    uint32_t length =
        static_cast<uint32_t>(length_buf[0]) | (static_cast<uint32_t>(length_buf[1]) << 8) |
        (static_cast<uint32_t>(length_buf[2]) << 16) | (static_cast<uint32_t>(length_buf[3]) << 24);

    if (length > 10 * 1024 * 1024)
    { // Защита от больших сообщений
	ec = boost::system::errc::make_error_code(boost::system::errc::message_size);
	return false;
    }

    std::vector<uint8_t> data(length);
    read(socket, buffer(data), ec);
    if (ec)
	return false;

    try
    {
	out_msg = deserialize_message(data);
    }
    catch (const std::exception& e)
    {
	ec = boost::system::errc::make_error_code(boost::system::errc::invalid_argument);
	return false;
    }

    return true;
}

// Поток, который в фоне читает все входящие сообщения от сервера
void background_reader(tcp::socket& socket)
{
    boost::system::error_code ec;
    while (true)
    {
	Message msg;
	if (!read_message(socket, msg, ec))
	{
	    if (ec)
	    {
		std::cerr << "[background_reader] read error: " << ec.message() << std::endl;
	    }
	    break; // выходим из цикла, если ошибка
	}
	// Выводим полученное сообщение в консоль
	std::cout << "[Got message] From: " << msg.sender << " | Text: " << msg.text
	          << " | Type: " << (int)msg.type << std::endl;
    }
}

int main(int argc, char* argv[])
{
    // Получаем имя пользователя из аргументов
    std::string username = (argc > 1) ? argv[1] : "user_default";

    // IP и порт сервера (можно изменить)
    std::string server_ip = "127.0.0.1";
    int server_port = SERVER_PORT; // из common.hpp

    // Создаём io_context и сокет
    boost::asio::io_context ioc;
    tcp::socket socket(ioc);

    // Разрешаем DNS-резолвинг
    tcp::resolver resolver(ioc);
    boost::system::error_code ec;
    auto endpoints = resolver.resolve(server_ip, std::to_string(server_port), ec);
    if (ec)
    {
	std::cerr << "Resolve error: " << ec.message() << std::endl;
	return 1;
    }

    // Подключаемся
    boost::asio::connect(socket, endpoints, ec);
    if (ec)
    {
	std::cerr << "Connect error: " << ec.message() << std::endl;
	return 1;
    }

    std::cout << "[Client] Connected to server " << server_ip << ":" << server_port << std::endl;

    // 1) Отправляем сообщение регистрации (ClientRegistration)
    {
	Message reg_msg;
	reg_msg.type = MessageType::ClientRegistration;
	reg_msg.sender = username;
	reg_msg.password = DEFAULT_PASSWORD;
	// Для сервера text = ip-адрес клиента, а file_size = порт клиента.
	// Но если клиент сам не слушает входящие TCP, можно поставить заглушки.
	reg_msg.text = "127.0.0.1";
	reg_msg.file_size = 9999; // любой "виртуальный" порт клиента

	if (!send_message(socket, reg_msg, ec))
	{
	    std::cerr << "[Client] Registration send error: " << ec.message() << std::endl;
	    return 1;
	}

	// Ждём ответ от сервера (синхронно)
	Message resp;
	if (!read_message(socket, resp, ec))
	{
	    std::cerr << "[Client] Registration response error: " << ec.message() << std::endl;
	    return 1;
	}
	std::cout << "[Client] Server response: " << resp.text << ", filename=" << resp.filename
	          << std::endl;
	if (resp.text == "Error")
	{
	    std::cerr << "[Client] Registration failed (wrong password or other error)\n";
	    return 1;
	}
    }

    // 2) Запускаем поток, который постоянно слушает входящие сообщения
    std::thread reader_thread([&socket]() { background_reader(socket); });

    // 3) (Опционально) Автоматически отправим тестовое сообщение другому пользователю:
    {
	Message msg;
	msg.type = MessageType::Text;
	msg.sender = username;
	msg.receiver = "user2"; // условно отправим пользователю "user2"
	msg.text = "Hello from " + username + "! (auto message)";

	if (!send_message(socket, msg, ec))
	{
	    std::cerr << "[Client] Send text message error: " << ec.message() << std::endl;
	}
	else
	{
	    std::cout << "[Client] Sent auto message to " << msg.receiver << std::endl;
	}
    }

    // Чтобы клиент не завершался сразу, ждём ввода (или делаем бесконечный цикл)
    std::cout << "[Client] Press ENTER to quit.\n";
    std::cin.get();

    // Закрываем сокет, завершаем поток
    socket.close();
    reader_thread.join();

    return 0;
}