#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <string>
#include <cstdint>
#include <stdexcept>
#include <sqlite3.h>

class Database {
public:
    Database(const std::string &db_path);
    ~Database();

    void init();
    bool add_user(const std::string &username, const std::string &password, const std::string &ip, uint16_t port);
    bool authenticate_and_update(const std::string &username, const std::string &password, const std::string &ip, uint16_t port);
    bool user_exists(const std::string &username);

private:
    sqlite3 *db_;
};

#endif
