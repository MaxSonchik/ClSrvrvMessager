# Компилятор и флаги
CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall -Wextra -g
# Пути к вашим заголовочным файлам (для компиляции)
PROJECT_INCLUDES = -I./src
# Пути к внешним include директориям (для компиляции)
# Убедитесь, что библиотеки установлены так, что их заголовки доступны через эти пути
EXTERNAL_INCLUDES = -I/usr/include -I/usr/local/include

# Объединяем все include пути для компиляции
INCLUDES = $(PROJECT_INCLUDES) $(EXTERNAL_INCLUDES)

# Пути и флаги линковщика
LDFLAGS = -L/usr/local/lib # Путь к библиотекам prometheus-cpp (если они там, проверьте!)
LIBS_SERVER = -lboost_system -lboost_thread -lprometheus-cpp-pull -lprometheus-cpp-core -lsqlite3 -largon2
LIBS_CLIENT = -lboost_system -lboost_thread

# Исходные файлы C++ (исключая GUI)
SERVER_SOURCES = $(wildcard src/main_tcp_server.cpp src/tcp/tcp_server.cpp src/database/database_manager.cpp src/common/json_util.cpp)
CLIENT_SOURCES = $(wildcard src/main_tcp_client.cpp src/tcp/tcp_client.cpp src/common/json_util.cpp)
ALL_CPP_SOURCES = $(SERVER_SOURCES) $(CLIENT_SOURCES)

# Объектные файлы (генерируются автоматически из .cpp)
SERVER_OBJS = $(SERVER_SOURCES:.cpp=.o)
CLIENT_OBJS = $(CLIENT_SOURCES:.cpp=.o)
ALL_OBJS = $(SERVER_OBJS) $(CLIENT_OBJS)

# Исполняемые файлы
SERVER_TARGET = messenger_tcp_server
CLIENT_TARGET = messenger_tcp_client
ALL_TARGETS = $(SERVER_TARGET) $(CLIENT_TARGET)

# --- Цели сборки ---
.PHONY: all
all: $(ALL_TARGETS)

$(SERVER_TARGET): $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS_SERVER)

$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS_CLIENT)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# --- Цели очистки ---
.PHONY: clean
clean:
	@echo "Cleaning object files, targets, and compile_commands.json..."
	rm -f $(ALL_TARGETS) $(ALL_OBJS) compile_commands.json

# --- Цели статического анализа и проверки include ---
CPP_STD := c++17
# Настройки для cppcheck
# Игнорируем системные пути и ненужные проверки
# Опция -i <path> игнорирует файлы из этого пути
CPPCHECK_IGNORE_PATHS := -i/usr/include/boost \
                         -i/usr/include/nlohmann \
                         -i/usr/include/prometheus \
                         -i/usr/include/sqlite3 \
                         -i/usr/include/argon2 \
                         -i/usr/local/include \
                         -isrc/iosgui
CPPCHECK_SUPPRESS := --suppress=missingIncludeSystem --suppress=unusedFunction
CPPCHECK_FLAGS := --enable=warning,performance,portability,style \
                  --std=$(CPP_STD) \
                  --language=c++ \
                  $(CPPCHECK_SUPPRESS)

.PHONY: cppcheck
cppcheck: compile_commands # Используем compile_commands.json для точности
	@echo "Running cppcheck and filtering for 'src/'..."
	@test -f compile_commands.json || { echo >&2 "Error: compile_commands.json not found. Run 'make compile_commands' first."; exit 1; }
	# Запускаем cppcheck и фильтруем вывод с помощью grep, показывая только строки, начинающиеся с 'src/'
	cppcheck $(CPPCHECK_FLAGS) --project=compile_commands.json 2>&1 | grep -E '^src/.*:' || echo "cppcheck finished with no issues found in src/ directory (or grep error)."
# Цель для генерации compile_commands.json с помощью bear
# Добавляем clean как зависимость, чтобы гарантировать пересборку
.PHONY: compile_commands
compile_commands: clean
	@echo "Generating compile_commands.json using bear (will trigger full rebuild)..."
	# Запускаем сборку через bear
	bear -- $(MAKE) all

# Цель для запуска iwyu (требует compile_commands.json)
.PHONY: iwyu
iwyu: compile_commands
	@echo "Running iwyu_tool.py (requires compile_commands.json)..."
	@command -v iwyu_tool.py >/dev/null 2>&1 || { echo >&2 "Error: iwyu_tool.py not found in PATH. Install iwyu correctly."; exit 1; }
	# Создаем пустой mapping файл, если его нет, т.к. iwyu_tool может его требовать
	@touch iwyu_mappings.imp
	iwyu_tool.py -p . --mapping_file=iwyu_mappings.imp -j $(nproc) || echo "IWYU finished with errors or recommendations."

# --- Прочие цели ---
.PHONY: clangfix
clangfix:
	@echo "Running clang-format..."
	clang-format -i src/tcp/*.cpp src/tcp/*.hpp src/database/*.cpp src/database/*.hpp src/common/*.cpp src/common/*.h src/main_*.cpp

# Объявляем все цели, которые не являются файлами
.PHONY: all clean cppcheck compile_commands iwyu clangfix
