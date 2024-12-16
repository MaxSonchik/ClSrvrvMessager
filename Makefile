CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 -I./include
LDFLAGS = -lboost_system -lpthread

SRCS = src/main_server.cpp src/main_client.cpp src/common.cpp src/stun.cpp src/stun_client.cpp src/tcp_server.cpp src/tcp_client.cpp src/udp_file_sender.cpp src/udp_file_receiver.cpp src/message.cpp src/file_transfer_protocol.cpp
OBJS = $(SRCS:.cpp=.o)

all: server client

server: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ src/main_server.o src/common.o src/stun.o src/stun_client.o src/tcp_server.o src/udp_file_sender.o src/udp_file_receiver.o src/message.o src/file_transfer_protocol.o $(LDFLAGS)

client: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ src/main_client.o src/common.o src/stun.o src/stun_client.o src/tcp_client.o src/udp_file_sender.o src/udp_file_receiver.o src/message.o src/file_transfer_protocol.o $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) server client
