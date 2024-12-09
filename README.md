# 🌐 Boost Asio Client-Server Application

![Boost](https://img.shields.io/badge/Boost-1.83-blue)
![C++](https://img.shields.io/badge/C%2B%2B-17-brightgreen)
![Status](https://img.shields.io/badge/Status-Working-success)

## 📜 Описание
Данное приложение демонстрирует реализацию клиент-серверного взаимодействия с использованием библиотеки **Boost.Asio**.  
Сервер принимает соединения по протоколу **TCP** и обрабатывает сообщения, а клиент подключается к серверу, отправляет и получает данные.

## 🗂 Структура проекта
- `boost_server.cpp` - Серверная часть.
- `boost_client.cpp` - Клиентская часть.

## 🚀 Требования
- **Boost Library** (v1.70+)
- **C++ Compiler** с поддержкой C++17 (например, GCC, MinGW, MSVC)
- **CMake** (для сборки проекта, опционально)

## 📦 Установка Boost
1. Установите библиотеку [Boost](https://www.boost.org/).
2. Компиляция для Windows:
    ```bash
    .\bootstrap.bat
    .\b2
    ```

## ⚙️ Компиляция и запуск

### 1. Компиляция сервера и клиента
Для сборки с помощью GCC:
```bash
g++ -std=c++17 -o server boost_server.cpp -lboost_system -pthread
g++ -std=c++17 -o client boost_client.cpp -lboost_system -pthread
