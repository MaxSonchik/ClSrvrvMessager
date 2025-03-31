CXX = g++
CXXFLAGS = -std=c++17 -pthread -I./src

# Source files
COMMON_SRCS = src/common/json_util.cpp
TCP_SRCS = src/tcp/tcp_server.cpp src/tcp/tcp_client.cpp
GRPC_SRCS = src/grpc/grpc_server.cpp src/grpc/grpc_client.cpp
MAIN_SRCS = src/main_tcp_server.cpp src/main_tcp_client.cpp src/main_grpc_server.cpp src/main_grpc_client.cpp src/main_benchmark.cpp

# Proto files
PROTO_FILE = src/grpc/chat.proto
PROTO_SRC_GRPC = src/grpc/chat.grpc.pb.cc
PROTO_SRC_CPP = src/grpc/chat.pb.cc
PROTO_HDR_GRPC = src/grpc/chat.grpc.pb.h
PROTO_HDR_CPP = src/grpc/chat.pb.h

# Targets
TARGETS = messenger_tcp_server messenger_tcp_client messenger_grpc_server messenger_grpc_client messenger_benchmark

all: $(TARGETS)

# Generate protobuf and gRPC code
$(PROTO_SRC_GRPC): $(PROTO_FILE)
	protoc -I src/grpc --cpp_out=src/grpc --grpc_out=src/grpc --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` $<

# Object files
OBJS_COMMON = $(COMMON_SRCS:.cpp=.o)
OBJS_TCP = $(TCP_SRCS:.cpp=.o)
OBJS_GRPC = $(GRPC_SRCS:.cpp=.o)
OBJS_PROTO = $(PROTO_SRC_GRPC:.cc=.o) $(PROTO_SRC_CPP:.cc=.o)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Build executables
messenger_tcp_server: $(OBJS_COMMON) $(OBJS_TCP) src/main_tcp_server.o
	$(CXX) $(CXXFLAGS) $^ -o $@ -lboost_system

messenger_tcp_client: $(OBJS_COMMON) $(OBJS_TCP) src/main_tcp_client.o
	$(CXX) $(CXXFLAGS) $^ -o $@ -lboost_system

messenger_grpc_server: $(OBJS_COMMON) $(OBJS_PROTO) $(OBJS_GRPC) src/main_grpc_server.o
	$(CXX) $(CXXFLAGS) $^ -o $@ `pkg-config --libs grpc++ protobuf` -lboost_system

messenger_grpc_client: $(OBJS_COMMON) $(OBJS_PROTO) $(OBJS_GRPC) src/main_grpc_client.o
	$(CXX) $(CXXFLAGS) $^ -o $@ `pkg-config --libs grpc++ protobuf` -lboost_system

messenger_benchmark: $(OBJS_COMMON) $(OBJS_TCP) $(OBJS_PROTO) $(OBJS_GRPC) src/main_benchmark.o
	$(CXX) $(CXXFLAGS) $^ -o $@ `pkg-config --libs grpc++ protobuf` -lboost_system

clean:
	rm -f $(TARGETS) src/**/*.o src/grpc/chat.pb.cc src/grpc/chat.grpc.pb.cc src/grpc/chat.pb.h src/grpc/chat.grpc.pb.h

clangfix:
	clang-format -i /src/tcp/*.cpp /src/tcp/*.hpp /src/grpc/*.cpp /src/grpc/*.hpp
