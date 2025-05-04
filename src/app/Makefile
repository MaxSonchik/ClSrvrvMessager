# # -----------------------------------------------------------------------------
# # Compiler and flags
# # -----------------------------------------------------------------------------
# GUI_DIR := ../GUI
# CXX        := g++
# CXXFLAGS   := -std=c++17 -pthread -Wall -Wextra -g -O2 \
#                -I./src \
#                -I/usr/include \
#                -I/usr/local/include \
#                -I/usr/local/include/prometheus \
#                -I/opt/local/include \
#                -I/opt/homebrew/include

# # -----------------------------------------------------------------------------
# # Linker flags and libraries
# # -----------------------------------------------------------------------------
# LDFLAGS    := -L/opt/homebrew/lib -L/opt/local/lib -L/usr/local/lib
# LIBS_SERVER := -lprometheus-cpp-pull -lprometheus-cpp-core \
#                -lpthread -lsqlite3 -largon2 -lstdc++ -lm
# LIBS_CLIENT := -lpthread -lstdc++

# # -----------------------------------------------------------------------------
# # Source file groups
# # -----------------------------------------------------------------------------
# COMMON_SRCS       := src/common/json_util.cpp
# DB_SRCS           := src/database/database_manager.cpp
# TCP_SRCS          := src/tcp/tcp_server.cpp src/tcp/tcp_client.cpp
# MAIN_TCP_SRCS     := src/main_tcp_server.cpp src/main_tcp_client.cpp

# # Derive object filenames
# OBJS_COMMON            := $(COMMON_SRCS:.cpp=.o)
# OBJS_DB                := $(DB_SRCS:.cpp=.o)
# OBJS_TCP               := $(TCP_SRCS:.cpp=.o)
# OBJS_MAIN_TCP_SERVER   := $(filter %main_tcp_server.o,$(MAIN_TCP_SRCS:.cpp=.o))
# OBJS_MAIN_TCP_CLIENT   := $(filter %main_tcp_client.o,$(MAIN_TCP_SRCS:.cpp=.o))

# # -----------------------------------------------------------------------------
# # Targets
# # -----------------------------------------------------------------------------
# SERVER_TARGET := messenger_tcp_server
# CLIENT_TARGET := messenger_tcp_client
# TARGETS       := $(SERVER_TARGET) $(CLIENT_TARGET)

# # -----------------------------------------------------------------------------
# # Default rule: build everything
# # -----------------------------------------------------------------------------
# .PHONY: all
# all: $(TARGETS)

# # -----------------------------------------------------------------------------
# # Compilation rule: .cpp → .o
# # -----------------------------------------------------------------------------
# %.o: %.cpp
# 	@echo "[CXX] $<"
# 	$(CXX) $(CXXFLAGS) -c $< -o $@

# # -----------------------------------------------------------------------------
# # Link server: includes DB & Prometheus
# # -----------------------------------------------------------------------------
# $(SERVER_TARGET): $(OBJS_COMMON) $(OBJS_DB) $(OBJS_TCP) $(OBJS_MAIN_TCP_SERVER)
# 	@echo "[LINK] $@"
# 	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS_SERVER)

# # -----------------------------------------------------------------------------
# # Link client: only COMMON + TCP client parts
# # -----------------------------------------------------------------------------
# $(CLIENT_TARGET): $(OBJS_COMMON) $(OBJS_MAIN_TCP_CLIENT) \
#                   $(filter-out src/tcp/tcp_server.o,$(OBJS_TCP))
# 	@echo "[LINK] $@"
# 	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS_CLIENT)

# # -----------------------------------------------------------------------------
# # Clean up build artifacts
# # -----------------------------------------------------------------------------
# .PHONY: clean
# clean:
# 	@echo "Cleaning build artifacts…"
# 	@rm -f $(TARGETS)
# 	@rm -f *.o
# 	@rm -f src/common/*.o src/database/*.o src/tcp/*.o

# # -----------------------------------------------------------------------------
# # Code formatting
# # -----------------------------------------------------------------------------
# .PHONY: clangfix
# clangfix:
# 	@echo "Running clang-format…"
# 	clang-format -i src/common/*.cpp src/common/*.h \
# 	              src/database/*.cpp src/database/*.hpp \
# 	              src/tcp/*.cpp src/tcp/*.hpp \
# 	              src/*.cpp

# # -----------------------------------------------------------------------------
# # Static analysis: cppcheck & compile_commands
# # -----------------------------------------------------------------------------
# CPP_STD               := c++17
# CPPCHECK_IGNORE_PATHS := -i/usr/include/boost \
#                          -i/usr/include/nlohmann \
#                          -i/usr/include/prometheus \
#                          -i/usr/include/sqlite3 \
#                          -i/usr/include/argon2 \
#                          -i/usr/local/include \
#                          -isrc/iosgui
# CPPCHECK_SUPPRESS     := --suppress=missingIncludeSystem --suppress=unusedFunction
# CPPCHECK_FLAGS        := --enable=warning,performance,portability,style \
#                          --std=$(CPP_STD) \
#                          --language=c++ \
#                          $(CPPCHECK_SUPPRESS)

# .PHONY: compile_commands
# compile_commands: clean
# 	@echo "Generating compile_commands.json via bear…"
# 	bear -- $(MAKE) all

# .PHONY: cppcheck
# cppcheck: compile_commands
# 	@echo "Running cppcheck for src/…"
# 	@test -f compile_commands.json || { echo >&2 "Error: compile_commands.json missing. Run 'make compile_commands' first."; exit 1; }
# 	cppcheck $(CPPCHECK_FLAGS) --project=compile_commands.json 2>&1 \
# 	  | grep -E '^src/.*:' || echo "No issues found in src/ or grep returned nothing."

# .PHONY: iwyu
# iwyu: compile_commands
# 	@echo "Running iwyu_tool.py…"
# 	@command -v iwyu_tool.py >/dev/null 2>&1 || { echo >&2 "Error: iwyu_tool.py not in PATH."; exit 1; }
# 	@touch iwyu_mappings.imp
# 	iwyu_tool.py -p . --mapping_file=iwyu_mappings.imp -j $(nproc) \
# 	  || echo "iwyu finished with recommendations or errors."

# .PHONY: gui
# gui:
# 	@echo "[GUI] building messenger_gui …"
# 	@cd $(GUI_DIR) && ./build_GUI.sh

# .PHONY: package
# package: all gui
# 	@echo "[PACKAGE] done."         # all = C++ сервер/клиент, gui = бинарник GUI



# -----------------------------------------------------------------------------
# Makefile for ClSrvrvMessager (Ubuntu/Debian Focused)
#
# Prerequisites (Ubuntu/Debian):
#   - build-essential         (Provides g++, make, etc.)
#   - g++-XX / libstdc++-XX-dev (Replace XX with desired version if default g++ is problematic, e.g., g++-11, libstdc++-11-dev)
#   - libprometheus-cpp-pull-dev
#   - libprometheus-cpp-core-dev
#   - libsqlite3-dev
#   - libargon2-dev
#   - libboost-dev            (Or specific boost libraries if needed)
#   - nlohmann-json3-dev      (If using system nlohmann json)
# Optional (for analysis/formatting):
#   - clang-format
#   - cppcheck
#   - bear
#   - python3-iwyu            (Or install iwyu_tool.py manually)
#
# Usage:
#   make                  # Build with default g++
#   make CXX=g++-11       # Build explicitly with g++-11 (ensure matching -dev installed)
#   make clean            # Remove build artifacts (REQUIRED before changing CXX)
#   make package          # Build binaries and GUI
#   make clangfix         # Format code
#   make cppcheck         # Run cppcheck (needs 'bear' and 'cppcheck')
#   make iwyu             # Run include-what-you-use (needs 'bear' and 'iwyu')
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# Compiler and flags
# -----------------------------------------------------------------------------
GUI_DIR := ../GUI

# Use '?=' to allow overriding CXX from command line (make CXX=g++-11)
CXX?= g++

# Cleaned up include paths for Linux
# -I/usr/local/* only needed if libraries/headers installed manually there.
CXXFLAGS := -std=c++17 -pthread -Wall -Wextra -g -O2 \
            -I./src \
            -I/usr/include \
            -I/usr/local/include \
            -I/usr/local/include/prometheus

# -----------------------------------------------------------------------------
# Linker flags and libraries
# -----------------------------------------------------------------------------
# Cleaned up library paths for Linux
# -L/usr/local/lib only needed if libraries installed manually there.
LDFLAGS    := -L/usr/local/lib

# Ensure corresponding -dev packages are installed (e.g., libprometheus-cpp-dev, etc.)
LIBS_SERVER := -lprometheus-cpp-pull -lprometheus-cpp-core \
               -lpthread -lsqlite3 -largon2 -lstdc++ -lm
LIBS_CLIENT := -lpthread -lstdc++ -lm # Added -lm for consistency, might be needed

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
# Use $(CXX) variable which might be overridden
%.o: %.cpp
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# -----------------------------------------------------------------------------
# Link server: includes DB & Prometheus
# -----------------------------------------------------------------------------
$(SERVER_TARGET): $(OBJS_COMMON) $(OBJS_DB) $(OBJS_TCP) $(OBJS_MAIN_TCP_SERVER)
	@echo "[LINK] $@"
	$(CXX) $^ -o $@ $(LDFLAGS) $(LIBS_SERVER) # Removed CXXFLAGS from link line, not strictly needed

# -----------------------------------------------------------------------------
# Link client: only COMMON + TCP client parts
# -----------------------------------------------------------------------------
$(CLIENT_TARGET): $(OBJS_COMMON) $(OBJS_MAIN_TCP_CLIENT) \
                  $(filter-out src/tcp/tcp_server.o,$(OBJS_TCP))
	@echo "[LINK] $@"
	$(CXX) $^ -o $@ $(LDFLAGS) $(LIBS_CLIENT) # Removed CXXFLAGS from link line

# -----------------------------------------------------------------------------
# Clean up build artifacts
# -----------------------------------------------------------------------------
.PHONY: clean
clean:
	@echo "Cleaning build artifacts…"
	@rm -f $(TARGETS)
	# Clean objects from source directories
	@rm -f src/common/*.o src/database/*.o src/tcp/*.o src/*.o

# -----------------------------------------------------------------------------
# Code formatting
# -----------------------------------------------------------------------------
.PHONY: clangfix
clangfix:
	@echo "Running clang-format…"
	@command -v clang-format >/dev/null 2>&1 || { echo >&2 "Error: clang-format not found. Install with 'sudo apt install clang-format'"; exit 1; }
	clang-format -i src/common/*.cpp src/common/*.h \
	              src/database/*.cpp src/database/*.hpp \
	              src/tcp/*.cpp src/tcp/*.hpp \
	              src/*.cpp src/*.hpp # Added src/*.hpp assuming headers might be there

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
	@command -v bear >/dev/null 2>&1 || { echo >&2 "Error: bear not found. Install with 'sudo apt install bear'"; exit 1; }
	# Pass the chosen compiler to the underlying make all command run by bear
	bear -- $(MAKE) all CXX=$(CXX)

.PHONY: cppcheck
cppcheck: compile_commands
	@echo "Running cppcheck for src/…"
	@command -v cppcheck >/dev/null 2>&1 || { echo >&2 "Error: cppcheck not found. Install with 'sudo apt install cppcheck'"; exit 1; }
	@test -f compile_commands.json || { echo >&2 "Error: compile_commands.json missing. Run 'make compile_commands' first."; exit 1; }
	cppcheck $(CPPCHECK_FLAGS) --project=compile_commands.json $(CPPCHECK_IGNORE_PATHS) 2>&1 \
	  | grep -E '^src/.*:' || echo "No issues found in src/ or grep returned nothing."

.PHONY: iwyu
iwyu: compile_commands
	@echo "Running iwyu_tool.py…"
	@command -v iwyu_tool.py >/dev/null 2>&1 || { echo >&2 "Error: iwyu_tool.py not found. Install with 'sudo apt install iwyu' or manually."; exit 1; }
	@command -v iwyu >/dev/null 2>&1 || { echo >&2 "Error: iwyu (the binary) not found. Install with 'sudo apt install iwyu'"; exit 1; }
	@touch iwyu_mappings.imp
	# Determine number of processors available
	NPROC=$(shell nproc || echo 1)
	iwyu_tool.py -p . --mapping_file=iwyu_mappings.imp -j $(NPROC) \
	  || echo "iwyu finished with recommendations or errors."

# -----------------------------------------------------------------------------
# GUI Build
# -----------------------------------------------------------------------------
.PHONY: gui
gui:
	@echo "[GUI] Building messenger_gui (requires separate setup in $(GUI_DIR))..."
	@if [ -d "$(GUI_DIR)" ] && [ -f "$(GUI_DIR)/build_GUI.sh" ]; then \
		cd $(GUI_DIR) && ./build_GUI.sh; \
	else \
		echo "[WARN] GUI directory or build script not found, skipping GUI build."; \
	fi

# -----------------------------------------------------------------------------
# Package target
# -----------------------------------------------------------------------------
.PHONY: package
package: all gui
	@echo "[PACKAGE] C++ build and GUI build attempt finished."
