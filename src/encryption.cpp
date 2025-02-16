#include "../include/encryption.hpp"

#include <argon2.h>

#include <algorithm>
#include <random>
#include <stdexcept>

namespace Security
{

HashResult generate_hash(const std::string& password)
{
    HashResult result;
    result.salt.resize(16);
    std::random_device rd;
    std::generate(result.salt.begin(), result.salt.end(), [&]() { return rd() % 256; });

    // Параметры Argon2
    uint32_t t_cost = 2;       // Итерации
    uint32_t m_cost = 1 << 16; // Память (64 MiB)
    uint32_t parallelism = 1;  // Потоки
    result.hash.resize(32);    // Размер хеша (32 байта)

    int res = argon2_hash(t_cost, m_cost, parallelism,
                          password.data(), // Пароль
                          password.size(),
                          result.salt.data(), // Соль
                          result.salt.size(),
                          result.hash.data(), // Выходной хеш
                          result.hash.size(),
                          nullptr,              // encoded (не используется)
                          0,                    // encodedlen (не используется)
                          Argon2_id,            // Тип алгоритма
                          ARGON2_VERSION_NUMBER // Версия
    );

    if (res != ARGON2_OK)
    {
	throw std::runtime_error("Argon2 error: " + std::string(argon2_error_message(res)));
    }

    return result;
}

bool verify_password(const std::string& password, const std::string& stored_hash,
                     const std::string& stored_salt)
{
    std::vector<unsigned char> computed_hash(stored_hash.size());

    int res = argon2_hash(2,               // t_cost
                          1 << 16,         // m_cost
                          1,               // parallelism
                          password.data(), // Пароль
                          password.size(),
                          stored_salt.data(), // Соль из БД
                          stored_salt.size(),
                          computed_hash.data(), // Вычисленный хеш
                          computed_hash.size(),
                          nullptr,              // encoded (не используется)
                          0,                    // encodedlen (не используется)
                          Argon2_id,            // Тип алгоритма
                          ARGON2_VERSION_NUMBER // Версия
    );

    // Преобразуем вектор в строку для сравнения
    std::string computed_hash_str(computed_hash.begin(), computed_hash.end());
    return (res == ARGON2_OK) && (computed_hash_str == stored_hash);
}

} // namespace Security