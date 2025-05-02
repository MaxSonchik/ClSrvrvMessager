# -----------------------------------------------------------------------------
# Compiler and flags
# -----------------------------------------------------------------------------
CXX        := g++
CXXFLAGS   := -std=c++17 -pthread -Wall -Wextra -g -O2 \
               -I./src \
               -I/usr/include \
               -I/usr/local/include \
               -I/usr/local/include/prometheus \
               -I/opt/local/include \
               -I/opt/homebrew/include

# -----------------------------------------------------------------------------
# Linker flags and libraries
# -----------------------------------------------------------------------------
LDFLAGS    := -L/opt/homebrew/lib -L/opt/local/lib -L/usr/local/lib
LIBS_SERVER := -lboost_system -lprometheus-cpp-pull -lprometheus-cpp-core \
               -lpthread -lsqlite3 -largon2 -lstdc++ -lm
LIBS_CLIENT := -lboost_system -lpthread -lstdc++

# -----------------------------------------------------------------------------
# Source file groups
# -----------------------------------------------------------------------------
COMMON_SRCS       := src/common/json_util.cpp
DB_SRCS           := src/database/database_manager.cpp
TCP_SRCS          := src/tcp/tcp_server.cpp src/tcp/tcp_client.cpp
MAIN_TCP_SRCS     := src/main_tcp_server.cpp src/main_tcp_client.cpp

# Derive object filenames
OBJS_COMMON            := $(COMMON_SRCS:.cpp=.o)
OBJS_DB                := $(DB_SRCS:.cpp=.o)
OBJS_TCP               := $(TCP_SRCS:.cpp=.o)
OBJS_MAIN_TCP_SERVER   := $(filter %main_tcp_server.o,$(MAIN_TCP_SRCS:.cpp=.o))
OBJS_MAIN_TCP_CLIENT   := $(filter %main_tcp_client.o,$(MAIN_TCP_SRCS:.cpp=.o))

# -----------------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------------
SERVER_TARGET := messenger_tcp_server
CLIENT_TARGET := messenger_tcp_client
TARGETS       := $(SERVER_TARGET) $(CLIENT_TARGET)

# -----------------------------------------------------------------------------
# Default rule: build everything
# -----------------------------------------------------------------------------
.PHONY: all
all: $(TARGETS)

# -----------------------------------------------------------------------------
# Compilation rule: .cpp → .o
# -----------------------------------------------------------------------------
%.o: %.cpp
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# -----------------------------------------------------------------------------
# Link server: includes DB & Prometheus
# -----------------------------------------------------------------------------
$(SERVER_TARGET): $(OBJS_COMMON) $(OBJS_DB) $(OBJS_TCP) $(OBJS_MAIN_TCP_SERVER)
	@echo "[LINK] $@"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS_SERVER)

# -----------------------------------------------------------------------------
# Link client: only COMMON + TCP client parts
# -----------------------------------------------------------------------------
$(CLIENT_TARGET): $(OBJS_COMMON) $(OBJS_MAIN_TCP_CLIENT) \
                  $(filter-out src/tcp/tcp_server.o,$(OBJS_TCP))
	@echo "[LINK] $@"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS_CLIENT)

# -----------------------------------------------------------------------------
# Clean up build artifacts
# -----------------------------------------------------------------------------
.PHONY: clean
clean:
	@echo "Cleaning build artifacts…"
	@rm -f $(TARGETS)
	@rm -f *.o
	@rm -f src/common/*.o src/database/*.o src/tcp/*.o

# -----------------------------------------------------------------------------
# Code formatting
# -----------------------------------------------------------------------------
.PHONY: clangfix
clangfix:
	@echo "Running clang-format…"
	clang-format -i src/common/*.cpp src/common/*.h \
	              src/database/*.cpp src/database/*.hpp \
	              src/tcp/*.cpp src/tcp/*.hpp \
	              src/*.cpp

# -----------------------------------------------------------------------------
# Static analysis: cppcheck & compile_commands
# -----------------------------------------------------------------------------
CPP_STD               := c++17
CPPCHECK_IGNORE_PATHS := -i/usr/include/boost \
                         -i/usr/include/nlohmann \
                         -i/usr/include/prometheus \
                         -i/usr/include/sqlite3 \
                         -i/usr/include/argon2 \
                         -i/usr/local/include \
                         -isrc/iosgui
CPPCHECK_SUPPRESS     := --suppress=missingIncludeSystem --suppress=unusedFunction
CPPCHECK_FLAGS        := --enable=warning,performance,portability,style \
                         --std=$(CPP_STD) \
                         --language=c++ \
                         $(CPPCHECK_SUPPRESS)

.PHONY: compile_commands
compile_commands: clean
	@echo "Generating compile_commands.json via bear…"
	bear -- $(MAKE) all

.PHONY: cppcheck
cppcheck: compile_commands
	@echo "Running cppcheck for src/…"
	@test -f compile_commands.json || { echo >&2 "Error: compile_commands.json missing. Run 'make compile_commands' first."; exit 1; }
	cppcheck $(CPPCHECK_FLAGS) --project=compile_commands.json 2>&1 \
	  | grep -E '^src/.*:' || echo "No issues found in src/ or grep returned nothing."

.PHONY: iwyu
iwyu: compile_commands
	@echo "Running iwyu_tool.py…"
	@command -v iwyu_tool.py >/dev/null 2>&1 || { echo >&2 "Error: iwyu_tool.py not in PATH."; exit 1; }
	@touch iwyu_mappings.imp
	iwyu_tool.py -p . --mapping_file=iwyu_mappings.imp -j $(nproc) \
	  || echo "iwyu finished with recommendations or errors."
