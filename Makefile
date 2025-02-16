CXX = g++
CXXFLAGS = -g -O0 -std=c++17 -Wall -Werror -Wextra -O2 -I./include -I/usr/include
LDFLAGS = -lboost_system -lpthread -lsqlite3 -largon2

SRCS_COMMON = src/common.cpp src/stun.cpp src/stun_client.cpp src/tcp_server.cpp src/tcp_client.cpp src/udp_file_sender.cpp src/udp_file_receiver.cpp src/message.cpp src/file_transfer_protocol.cpp src/database.cpp src/encryption.cpp
HEADERS_COMMON = include/common.hpp include/stun.hpp include/stun_client.hpp include/tcp_server.hpp include/tcp_client.hpp include/udp_file_sender.hpp include/udp_file_receiver.hpp include/message.hpp include/file_transfer_protocol.hpp include/database.hpp include/encryption.hpp
SRCS_SERVER = src/main_server.cpp $(SRCS_COMMON)
SRCS_CLIENT = src/main_client.cpp $(SRCS_COMMON)

OBJS_SERVER = $(SRCS_SERVER:.cpp=.o)
OBJS_CLIENT = $(SRCS_CLIENT:.cpp=.o)
LINTER_FILES = server_asan server_sanitize server_tsan server_ubsan valgrind_server client_sanitize client_tsan client_asan client_ubsan

all: server client

server: $(OBJS_SERVER)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS_SERVER) $(LDFLAGS) 

client: $(OBJS_CLIENT)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS_CLIENT) $(LDFLAGS)  

tests: $(filter-out src/main_server.o, $(OBJS_SERVER)) tests/test.cpp
	$(CXX) $(CXXFLAGS) tests/test.cpp $(filter-out src/main_server.o, $(OBJS_SERVER)) -o test $(LDFLAGS) -lboost_unit_test_framework
	./test

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS_SERVER) $(OBJS_CLIENT) server client test $(LINTERFILES)

# ===================== ЛИНТЕРЫ =====================


#clang-format
check:
	clang-format -n $(SRCS_COMMON) $(HEADERS_COMMON)

clangfix:
	clang-format -i $(SRCS_COMMON) $(HEADERS_COMMON) src/main_server.cpp src/main_client.cpp tests/test.cpp

clangcheck:
	clang-format --dry-run --Werror $(SRCS_COMMON) $(HEADERS_COMMON) src/main_server.cpp src/main_client.cpp tests/test.cpp


# Cppcheck (статический анализ)
cppcheck:
	cppcheck --enable=all --inconclusive --force --std=c++17 --suppress=missingIncludeSystem $(SRCS_COMMON) src/main_server.cpp src/main_client.cpp

# Запуск сервера с Valgrind
valgrind_server: server
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=valgrind_server.log ./server

# Запуск клиента с Valgrind
valgrind_client: client
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=valgrind_client.log ./client

# Компиляция с AddressSanitizer (ASan)
asan_server:
	$(CXX) $(CXXFLAGS) -fsanitize=address -g -o server_asan $(SRCS_SERVER) $(LDFLAGS)

asan_client:
	$(CXX) $(CXXFLAGS) -fsanitize=address -g -o client_asan $(SRCS_CLIENT) $(LDFLAGS)

# Компиляция с UndefinedBehaviorSanitizer (UBSan)
ubsan_server:
	$(CXX) $(CXXFLAGS) -fsanitize=undefined -g -o server_ubsan $(SRCS_SERVER) $(LDFLAGS)

ubsan_client:
	$(CXX) $(CXXFLAGS) -fsanitize=undefined -g -o client_ubsan $(SRCS_CLIENT) $(LDFLAGS)

# Компиляция с ThreadSanitizer (TSan) — только для многопоточного кода
tsan_server:
	$(CXX) $(CXXFLAGS) -fsanitize=thread -g -o server_tsan $(SRCS_SERVER) $(LDFLAGS)

tsan_client:
	$(CXX) $(CXXFLAGS) -fsanitize=thread -g -o client_tsan $(SRCS_CLIENT) $(LDFLAGS)

# Компиляция с полной защитой (ASan + UBSan)
full_sanitize_server:
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined -g -o server_sanitize $(SRCS_SERVER) $(LDFLAGS)

full_sanitize_client:
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined -g -o client_sanitize $(SRCS_CLIENT) $(LDFLAGS)
