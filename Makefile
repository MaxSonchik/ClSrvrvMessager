# Компилятор и флаги
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Werror -Wextra -g -fpermissive -pthread
LDFLAGS = -lboost_system -lsqlite3

# Исходные файлы
SERVER_SRC = boost_server.cpp
CLIENT_SRC = boost_client.cpp
UTILS_SRC = stun_utils.cpp

# Объектные файлы
SERVER_OBJ = $(SERVER_SRC:.cpp=.o)
CLIENT_OBJ = $(CLIENT_SRC:.cpp=.o)
UTILS_OBJ = $(UTILS_SRC:.cpp=.o)

# Исполнимые файлы
SERVER_BIN = server
CLIENT_BIN = client

.PHONY: all clean clang

# Цель по умолчанию (сборка всего)
all: $(SERVER_BIN) $(CLIENT_BIN)

# Правила для сборки сервера
$(SERVER_BIN): $(SERVER_OBJ) $(UTILS_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Правила для сборки клиента
$(CLIENT_BIN): $(CLIENT_OBJ) $(UTILS_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Правило для компиляции исходников в объектные файлы
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Цель для чистки сгенерированных файлов
clean:
	rm -f $(SERVER_OBJ) $(CLIENT_OBJ) $(UTILS_OBJ) $(SERVER_BIN) $(CLIENT_BIN)

# Цель для форматирования с использованием clang-format
clang-check:
	find . -name "*.cpp" | xargs clang-format -n

clang-format:
	find . -name "*.cpp" | xargs clang-format -i

# Отладочная цель
debug: CXXFLAGS += -DDEBUG
debug: $(SERVER_BIN) $(CLIENT_BIN)
