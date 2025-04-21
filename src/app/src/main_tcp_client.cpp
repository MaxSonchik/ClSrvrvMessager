#include "tcp/tcp_client.hpp"
#include <iostream>
#include <string>
#include <stdexcept>
#include <thread> // Для sleep_for

// Helper для вывода JSON в stdout
void output_json(const tcp_messenger::json& j) {
    std::cout << j.dump() << std::endl; // endl для flush
}

// --- Обновленные коллбэки для вывода JSON ---
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
     // Запрос списка задач может быть инициирован из GUI после получения login_result
}

void handle_message_received(const std::string& from, const std::string& text) {
    output_json({
        {"type", "message"},
        {"from", from},
        {"text", text}
    });
}

void handle_task_list(const tcp_messenger::json& task_list_json) {
    // Пересылаем JSON от сервера как есть, добавив свой тип
    tcp_messenger::json output = task_list_json;
    output["type"] = "task_list_result";
     output_json(output);
}

void handle_task_notification(const tcp_messenger::json& notification_json) {
    // Пересылаем JSON от сервера как есть, добавив свой тип
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
}

int main(int argc, char* argv[]) {
    // Ожидаем аргументы: <host> <port>
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <server_host> <server_port>" << std::endl;
         // Вывод ошибки в JSON формате для GUI
         tcp_messenger::json err_json = {
             {"type", "client_error"},
             {"message", "Invalid command line arguments. Usage: messenger_tcp_client <host> <port>"}
         };
         std::cerr << err_json.dump() << std::endl;
        return 1;
    }

    std::string host = argv[1];
    unsigned short port;
    try {
        port = std::stoi(argv[2]);
         if (port == 0 || port > 65535) throw std::invalid_argument("Invalid port number");
    } catch (const std::exception& e) {
        std::cerr << "Invalid port number: " << argv[2] << " (" << e.what() << ")" << std::endl;
         tcp_messenger::json err_json = {
             {"type", "client_error"},
             {"message", "Invalid port number: " + std::string(argv[2]) + " - " + e.what()}
         };
         std::cerr << err_json.dump() << std::endl;
        return 1;
    }

    try {
        // Создаем клиент
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

             // Выводим лог о полученной команде (в JSON)
             // output_json({{"type", "log"}, {"level", "info"}, {"message", "Received command: " + line}});

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
                    client.login(cmd_json.at("username"), cmd_json.at("password"));
                } else if (command == "send_message") {
                    client.send_message(cmd_json.at("to"), cmd_json.at("text"));
                } else if (command == "add_task") {
                     client.add_task(cmd_json.at("task_name"),
                                     cmd_json.at("description"),
                                     cmd_json.at("trigger_time"), // DD.MM.YYYY HH:MM
                                     cmd_json.at("notify_offset"));
                } else if (command == "list_tasks") {
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
            } catch (const tcp_messenger::json::exception& e) { // Ошибки доступа к полям (at())
                 std::cerr << "Invalid command JSON structure: " << e.what() << ", input: [" << line << "]" << std::endl;
                 output_json({{"type", "client_error"}, {"message", "Invalid command JSON structure: " + std::string(e.what())}});
            } catch (const std::exception& e) {
                 std::cerr << "Error processing command: " << e.what() << ", input: [" << line << "]" << std::endl;
                 output_json({{"type", "client_error"}, {"message", "Error processing command: " + std::string(e.what())}});
            }
        }

        // Останавливаем клиент после завершения цикла stdin или команды stop
        client.stop();

    } catch (const std::exception& e) {
        std::cerr << "Client critical error: " << e.what() << std::endl;
        output_json({{"type", "client_error"}, {"message", "Client critical error: " + std::string(e.what())}});
        return 1;
    } catch (...) {
        std::cerr << "An unknown critical error occurred." << std::endl;
        output_json({{"type", "client_error"}, {"message", "Unknown critical error."}});
        return 1;
    }

    // output_json({{"type", "log"}, {"level", "info"}, {"message", "Client finished."}});
    return 0;
}
