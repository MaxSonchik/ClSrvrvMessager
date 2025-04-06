# Компилятор и флаги
CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall -Wextra -g # Добавлены флаги предупреждений и отладки
INCLUDES = -I./src -I/usr/local/include # Объединяем пути включения
LDFLAGS = -L/usr/local/lib # Путь к библиотекам prometheus-cpp
LIBS_SERVER = -lboost_system -lboost_thread -lprometheus-cpp-pull -lprometheus-cpp-core -lsqlite3 # Добавлена -lsqlite3 и boost::thread
LIBS_CLIENT = -lboost_system -lboost_thread # Клиенту не нужны prometheus или sqlite

# Исходные файлы
SERVER_SOURCES = src/main_tcp_server.cpp src/tcp/tcp_server.cpp src/database/database_manager.cpp src/common/json_util.cpp
CLIENT_SOURCES = src/main_tcp_client.cpp src/tcp/tcp_client.cpp src/common/json_util.cpp

# Объектные файлы (генерируются автоматически из .cpp)
SERVER_OBJS = $(SERVER_SOURCES:.cpp=.o)
CLIENT_OBJS = $(CLIENT_SOURCES:.cpp=.o)

# Исполняемые файлы
SERVER_TARGET = messenger_tcp_server
CLIENT_TARGET = messenger_tcp_client

# Цель по умолчанию: собрать все
all: $(SERVER_TARGET) $(CLIENT_TARGET)

# Правило для сборки сервера
$(SERVER_TARGET): $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) $(SERVER_OBJS) -o $(SERVER_TARGET) $(LDFLAGS) $(LIBS_SERVER)

# Правило для сборки клиента
$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) $(CLIENT_OBJS) -o $(CLIENT_TARGET) $(LDFLAGS) $(LIBS_CLIENT)

# Общее правило для компиляции .cpp в .o
# $(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
# $< - имя исходного файла (первая зависимость)
# $@ - имя цели (файла .o)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Очистка
clean:
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET) $(SERVER_OBJS) $(CLIENT_OBJS)


clangfix:
	clang-format -i /src/tcp/*.cpp /src/tcp/*.hpp /src/grpc/*.cpp /src/grpc/*.hpp

.PHONY: all clean