#include "../include/common.hpp"

#include <iostream>
void log_info(const std::string &msg) {
    std::cout << "[INFO] " << msg << "\n";
}
void log_error(const std::string &msg) {
    std::cerr << "[ERROR] " << msg << "\n";
}
