# Параметры компиляции
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Werror -Wextra -g -fpermissive -lboost_system -pthread

# Исходные файлы
SERVER_SRC = boost_server.cpp
CLIENT_SRC = boost_client.cpp

# Объектные файлы
SERVER_OBJ = boost_server.o
CLIENT_OBJ = boost_client.o

# Исполнимые файлы
SERVER_BIN = server
CLIENT_BIN = client

.PHONY: all clean clang-format

# Цель по умолчанию (сборка всего)
all: $(SERVER_BIN) $(CLIENT_BIN)

# Правила для сборки сервера
$(SERVER_BIN): $(SERVER_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Правила для сборки клиента
$(CLIENT_BIN): $(CLIENT_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Правило для компиляции исходников в объектные файлы
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Цель для чистки сгенерированных файлов
clean:
	rm -f $(SERVER_OBJ) $(CLIENT_OBJ) $(SERVER_BIN) $(CLIENT_BIN)

# Цель для форматирования с использованием clang-format
clang-format:
	clang-format -i *.cpp *.h

# Отладочная цель
debug: CXXFLAGS += -DDEBUG
debug: $(SERVER_BIN) $(CLIENT_BIN)
