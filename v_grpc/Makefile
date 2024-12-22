# Указать компилятор
CXX = g++
CXX_FLAGS = -std=c++17 -Wall -Wextra -O2

# Папки проекта
PROTO_DIR = proto
CLIENT_DIR = client
SERVER_DIR = server
BUILD_DIR = build

# Прото-файлы
PROTO_SRC = $(PROTO_DIR)/messenger.proto
PROTO_GEN = $(BUILD_DIR)/messenger.pb.cc $(BUILD_DIR)/messenger.grpc.pb.cc

# Библиотеки
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`
LIBS = -lgrpc++ -lprotobuf -lpthread

# Таргеты
CLIENT_BIN = $(BUILD_DIR)/client
SERVER_BIN = $(BUILD_DIR)/server

# Основные цели
all: $(CLIENT_BIN) $(SERVER_BIN)

# Сборка клиента
$(CLIENT_BIN): $(CLIENT_DIR)/client.cpp $(PROTO_GEN)
	$(CXX) $(CXX_FLAGS) -I$(BUILD_DIR) -o $@ $^ $(LIBS)

# Сборка сервера
$(SERVER_BIN): $(SERVER_DIR)/server.cpp $(PROTO_GEN)
	$(CXX) $(CXX_FLAGS) -I$(BUILD_DIR) -o $@ $^ $(LIBS)

# Генерация protobuf и gRPC файлов
$(BUILD_DIR)/%.pb.cc $(BUILD_DIR)/%.grpc.pb.cc: $(PROTO_SRC) | $(BUILD_DIR)
	protoc -I=$(PROTO_DIR) --grpc_out=$(BUILD_DIR) --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<
	protoc -I=$(PROTO_DIR) --cpp_out=$(BUILD_DIR) $<

# Создание папки для сборки
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Очистка проекта
clean:
	rm -rf $(BUILD_DIR)

# Удаление только бинарников
clean-bin:
	rm -f $(CLIENT_BIN) $(SERVER_BIN)

.PHONY: all clean clean-bin
