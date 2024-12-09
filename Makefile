# Параметры компиляции
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Werror -Wextra -g -fpermissive -pthread
LDFLAGS = -lboost_system -lsqlite3

# Исходные файлы
SERVER_SRC = boost_server.cpp
CLIENT_SRC = boost_client.cpp

# Объектные файлы
SERVER_OBJ = boost_server.o
CLIENT_OBJ = boost_client.o

# Исполнимые файлы
SERVER_BIN = server
CLIENT_BIN = client

.PHONY: all clean clang

# Цель по умолчанию (сборка всего)
all: $(SERVER_BIN) $(CLIENT_BIN)

# Правила для сборки сервера
$(SERVER_BIN): $(SERVER_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Правила для сборки клиента
$(CLIENT_BIN): $(CLIENT_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Правило для компиляции исходников в объектные файлы
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Цель для чистки сгенерированных файлов
clean:
	rm -f $(SERVER_OBJ) $(CLIENT_OBJ) $(SERVER_BIN) $(CLIENT_BIN)

# Цель для форматирования с использованием clang-format
clang-check:
	find . -name "*.cpp" | xargs clang-format -n

clang-format:
	find . -name "*.cpp" | xargs clang-format -i

# Отладочная цель
debug: CXXFLAGS += -DDEBUG
debug: $(SERVER_BIN) $(CLIENT_BIN)
