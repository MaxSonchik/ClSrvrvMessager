#include "../include/common.hpp"

#include <iostream>
<<<<<<< HEAD
/**
 * @brief Выводит информационное сообщение.
 *
 * Эта функция предназначена для вывода информационных сообщений в консоль
 * с пометкой [INFO]. Сообщение выводится через стандартный поток вывода.
 *
 * @param msg Сообщение, которое будет выведено. Тип: std::string.
 */
void log_info(const std::string &msg) {
    std::cout << "[INFO] " << msg << "\n";
}
/**
 * @brief Выводит сообщение об ошибке.
 *
 * Эта функция предназначена для вывода сообщений об ошибках в консоль
 * с пометкой [ERROR]. Сообщение выводится через стандартный поток ошибок.
 *
 * @param msg Сообщение об ошибке, которое будет выведено. Тип: std::string.
 */
void log_error(const std::string &msg) {
    std::cerr << "[ERROR] " << msg << "\n";
}
=======

void log_info(const std::string &msg) { std::cout << "[INFO] " << msg << "\n"; }

void log_error(const std::string &msg) { std::cerr << "[ERROR] " << msg << "\n"; }
>>>>>>> 5b7bf873fc0ad5481df1d8d45a6690206b0d4f47
