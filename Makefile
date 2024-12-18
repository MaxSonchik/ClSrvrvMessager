CXX = g++
CXXFLAGS = -std=c++17 -Wall -Werror -Wextra -O2 -I./include
LDFLAGS = -lboost_system -lpthread -lsqlite3

SRCS_COMMON = src/common.cpp src/stun.cpp src/stun_client.cpp src/tcp_server.cpp src/tcp_client.cpp src/udp_file_sender.cpp src/udp_file_receiver.cpp src/message.cpp src/file_transfer_protocol.cpp src/database.cpp src/encryption.cpp
SRCS_SERVER = src/main_server.cpp $(SRCS_COMMON)
SRCS_CLIENT = src/main_client.cpp $(SRCS_COMMON)

OBJS_SERVER = $(SRCS_SERVER:.cpp=.o)
OBJS_CLIENT = $(SRCS_CLIENT:.cpp=.o)

all: server client

server: $(OBJS_SERVER)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS_SERVER) $(LDFLAGS)

client: $(OBJS_CLIENT)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS_CLIENT) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS_SERVER) $(OBJS_CLIENT) server client
