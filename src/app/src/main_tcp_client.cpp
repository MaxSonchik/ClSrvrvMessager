//main_tcp_clent.cpp

#include "tcp/tcp_client.hpp"
#include <iostream>
#include <string>
#include <stdexcept>
#include <thread> // Для sleep_for
#include <limits> // Для numeric_limits - нужно для очистки cin после ошибки порта

// ---> Глобальная переменная для хранения имени текущего пользователя <---
std::string current_logged_in_user = "";
// TODO: В реальном GUI это будет управляться более надежно (например, в состоянии приложения)

// Helper для вывода JSON в stdout
void output_json(const tcp_messenger::json& j) {
    std::cout << j.dump() << std::endl;
    std::cout.flush(); // Принудительный сброс буфера
}

// --- Коллбэки для обработки ответов от клиента ---

void handle_register_result(bool success, const std::string& message) {
    output_json({
        {"type", "register_result"},
        {"success", success},
        {"message", message}
    });
}

void handle_login_result(bool success, const std::string& message) {
     output_json({
        {"type", "login_result"},
        {"success", success},
        {"message", message}
    });
     // Важно: current_logged_in_user НЕ устанавливается здесь,
     // он устанавливается в main перед вызовом client.login()
     // и сбрасывается здесь только при ошибке
     if (!success) {
         current_logged_in_user = "";
     }
}

void handle_message_received(const std::string& from, const std::string& text) {
    output_json({
        {"type", "message"},
        {"from", from},
        {"to", current_logged_in_user}, // Добавляем текущего пользователя как получателя
        {"text", text}
    });
}

void handle_task_list(const tcp_messenger::json& task_list_json) {
    tcp_messenger::json output = task_list_json;
    output["type"] = "task_list_result";
     output_json(output);
}

void handle_task_notification(const tcp_messenger::json& notification_json) {
    tcp_messenger::json output = notification_json;
    output["type"] = "task_notification";
    output_json(output);
}

void handle_server_error(const std::string& error_message) {
     output_json({
        {"type", "server_error"},
        {"message", error_message}
    });
}

void handle_server_status(const std::string& status_message) {
     output_json({
        {"type", "server_status"},
        {"message", status_message}
    });
}

void handle_connection_status(bool is_connected) {
    output_json({
        {"type", "connection_status"},
        {"connected", is_connected}
    });
    if (!is_connected) {
        // Сбрасываем пользователя при дисконнекте
        current_logged_in_user = "";
    }
}

// --- Основная функция ---

//! ЗАПУСК ПРОГРАММЫ ПРОИСХОДИТ ПУТЕМ ./messenger_tcp_client <IP> <порт>
int main(int argc, char* argv[]) {
    // --- Парсинг аргументов командной строки ---
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <server_host> <server_port>" << std::endl;
         tcp_messenger::json err_json = {
             {"type", "client_error"},
             {"message", "Invalid command line arguments. Usage: messenger_tcp_client <host> <port>"}
         };
         std::cerr << err_json.dump() << std::endl;
        return 1;
    }

    std::string host = argv[1];
    unsigned short port = 0; // Инициализируем нулем
    try {
        int port_val = std::stoi(argv[2]); // Сначала читаем в int для полной проверки диапазона
        // Проверка диапазона 1-65535
        if (port_val <= 0 || port_val > 65535) {
            throw std::out_of_range("Port number out of range (1-65535)");
        }
        port = static_cast<unsigned short>(port_val); // Присваиваем, если в диапазоне
    } catch (const std::exception& e) {
        std::cerr << "Invalid port number: " << argv[2] << " (" << e.what() << ")" << std::endl;
         tcp_messenger::json err_json = {
             {"type", "client_error"},
             {"message", "Invalid port number: " + std::string(argv[2]) + " - " + e.what()}
         };
         std::cerr << err_json.dump() << std::endl;
        return 1;
    }
    // --- Конец парсинга аргументов ---


    // --- Основной блок try/catch для работы клиента ---
    try {
        // Создаем клиент с полученными host и port
        tcp_messenger::TCPClient client(host, port);

        // Назначаем коллбэки
        client.on_register_result = handle_register_result;
        client.on_login_result = handle_login_result;
        client.on_message_received = handle_message_received;
        client.on_task_list_received = handle_task_list;
        client.on_task_notification = handle_task_notification;
        client.on_server_error = handle_server_error;
        client.on_server_status = handle_server_status;
        client.on_connection_status_changed = handle_connection_status;

        // Запускаем клиент (потоки и подключение)
        client.start();

        // Цикл чтения команд из stdin
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;

            try {
                tcp_messenger::json cmd_json = tcp_messenger::json::parse(line);
                if (!cmd_json.contains("command") || !cmd_json["command"].is_string()) {
                    throw std::runtime_error("Missing or invalid 'command' field in input JSON");
                }
                std::string command = cmd_json["command"];

                // Маршрутизация команды
                if (command == "register") {
                    client.register_user(cmd_json.at("username"), cmd_json.at("password"));
                } else if (command == "login") {
                    current_logged_in_user = cmd_json.at("username"); // Сохраняем локально ПЕРЕД отправкой
                    client.login(current_logged_in_user, cmd_json.at("password"));
                } else if (command == "send_message") {
                     if (current_logged_in_user.empty()) {
                          throw std::runtime_error("Cannot send message: Not logged in.");
                     }
                    client.send_message(cmd_json.at("to"), cmd_json.at("text"));
                } else if (command == "add_task") {
                     if (current_logged_in_user.empty()) { throw std::runtime_error("Cannot add task: Not logged in."); }
                     client.add_task(cmd_json.at("task_name"),
                                     cmd_json.at("description"),
                                     cmd_json.at("trigger_time"), // DD.MM.YYYY HH:MM
                                     cmd_json.at("notify_offset"));
                } else if (command == "list_tasks") {
                     if (current_logged_in_user.empty()) { throw std::runtime_error("Cannot list tasks: Not logged in."); }
                     client.request_task_list();
                } else if (command == "stop") {
                     output_json({{"type", "log"}, {"level", "info"}, {"message", "Stop command received. Exiting."}});
                     break; // Выход из цикла чтения stdin
                }
                // TODO: Добавить команду send_file_offer
                else {
                     throw std::runtime_error("Unknown command: " + command);
                }

            } catch (const tcp_messenger::json::parse_error& e) {
                 std::cerr << "Failed to parse command JSON: " << e.what() << ", input: [" << line << "]" << std::endl;
                 output_json({{"type", "client_error"}, {"message", "Failed to parse command JSON: " + std::string(e.what())}});
            } catch (const tcp_messenger::json::exception& e) { // Ошибки доступа к полям (at()) или типа
                 std::cerr << "Invalid command JSON structure or data type: " << e.what() << ", input: [" << line << "]" << std::endl;
                  // Сбрасываем пользователя на всякий случай при ошибке парсинга команды логина
                 if (line.find("\"command\": \"login\"") != std::string::npos) {
                     current_logged_in_user = "";
                 }
                 output_json({{"type", "client_error"}, {"message", "Invalid command JSON structure/type: " + std::string(e.what())}});
            } catch (const std::exception& e) { // Другие ошибки обработки
                 std::cerr << "Error processing command: " << e.what() << ", input: [" << line << "]" << std::endl;
                 output_json({{"type", "client_error"}, {"message", "Error processing command: " + std::string(e.what())}});
            }
        } // Конец while(getline)

        // Останавливаем клиент после завершения цикла stdin (Ctrl+D) или команды stop
        client.stop();

    // Важно: } закрывает основной блок try
    } catch (const std::exception& e) {
        std::cerr << "Client critical error (e.g., during initialization): " << e.what() << std::endl;
        output_json({{"type", "client_error"}, {"message", "Client critical error: " + std::string(e.what())}});
        return 1;
    } catch (...) {
        std::cerr << "An unknown critical error occurred." << std::endl;
        output_json({{"type", "client_error"}, {"message", "Unknown critical error."}});
        return 1;
    }

    // Успешное завершение программы
    return 0;
}
