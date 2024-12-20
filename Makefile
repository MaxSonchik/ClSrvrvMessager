CXX = g++
CXXFLAGS = -std=c++17 -Wall -Werror -Wextra -O2 -I./include
LDFLAGS = -lboost_system -lpthread -lsqlite3

SRCS_COMMON = src/common.cpp src/stun.cpp src/stun_client.cpp src/tcp_server.cpp src/tcp_client.cpp src/udp_file_sender.cpp src/udp_file_receiver.cpp src/message.cpp src/file_transfer_protocol.cpp src/database.cpp src/encryption.cpp
HEADERS_COMMON = include/common.hpp include/stun.hpp include/stun_client.hpp include/tcp_server.hpp include/tcp_client.hpp include/udp_file_sender.hpp include/udp_file_receiver.hpp include/message.hpp include/file_transfer_protocol.hpp include/database.hpp include/encryption.hpp
SRCS_SERVER = src/main_server.cpp $(SRCS_COMMON)
SRCS_CLIENT = src/main_client.cpp $(SRCS_COMMON)

OBJS_SERVER = $(SRCS_SERVER:.cpp=.o)
OBJS_CLIENT = $(SRCS_CLIENT:.cpp=.o)

all: server client

server: $(OBJS_SERVER)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS_SERVER) $(LDFLAGS)

client: $(OBJS_CLIENT)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS_CLIENT) $(LDFLAGS)

tests: $(filter-out src/main_server.o, $(OBJS_SERVER)) tests/test.cpp
	$(CXX) $(CXXFLAGS) tests/test.cpp $(filter-out src/main_server.o, $(OBJS_SERVER)) -o test $(LDFLAGS) -lboost_unit_test_framework
	./test

clangfix:
	clang-format -i $(SRCS_COMMON) $(HEADERS_COMMON) src/main_server.cpp src/main_client.cpp tests/test.cpp

clangcheck:
	clang-format --dry-run --Werror $(SRCS_COMMON) $(HEADERS_COMMON) src/main_server.cpp src/main_client.cpp tests/test.cpp

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS_SERVER) $(OBJS_CLIENT) server client test