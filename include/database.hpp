#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <sqlite3.h>

#include <cstdint> // Для uint16_t
#include <stdexcept>
#include <string>

class Database
{
  public:
    Database(const std::string& db_path);
    ~Database();

    void init();
    bool is_initialized() const;

    bool user_exists(const std::string& username);
    bool create_user(const std::string& username, const std::string& password_hash,
                     const std::string& salt);
    bool authenticate_user(const std::string& username, const std::string& password);
    bool authenticate_and_update(const std::string& username, const std::string& password,
                                 const std::string& ip, uint16_t port);
    bool update_connection_info(const std::string& username, const std::string& ip, uint16_t port);

    bool save_message(const std::string& sender, const std::string& receiver,
                      const std::string& text);

  private:
    sqlite3* db_;
    std::string db_path_;
    bool is_initialized_;

    void execute_query(const std::string& query);
    void check_db_connection() const;
};

#endif // DATABASE_HPP