#ifndef DATABASE_HPP
#define DATABASE_HPP

#include "../include/encryption.hpp"
#include <cstdint>
#include <iostream>
#include <sqlite3.h>
#include <stdexcept>
#include <string>

class Database
{
  public:
    explicit Database(const std::string& db_path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) = delete;
    Database& operator=(Database&&) = delete;

    bool user_exists(const std::string& username);
    bool create_user(const std::string& username, const std::string& password_hash,
                     const std::string& salt);
    bool authenticate_user(const std::string& username, const std::string& password);
    bool update_connection_info(const std::string& username, const std::string& ip,
                                std::uint16_t port);
    bool authenticate_and_update(const std::string& username, const std::string& password,
                                 const std::string& ip, std::uint16_t port);
    bool save_message(const std::string& sender, const std::string& receiver,
                      const std::string& text);
    bool is_initialized() const;

  private:
    void init();
    void execute_query(const std::string& query);
    void check_db_connection() const;

    sqlite3* db_;
    std::string db_path_;
    bool is_initialized_;
};

#endif // DATABASE_HPP