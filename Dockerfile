# Базовый образ с поддержкой g++ и необходимых библиотек
FROM ubuntu:22.04

# Устанавливаем зависимости
RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    make \
    libboost-system-dev \
    libsqlite3-dev \
    libpthread-stubs0-dev \
    libboost-filesystem-dev \
    libboost-program-options-dev \
    libboost-thread-dev \
    libboost-regex-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/* && apt-get clean

# Устанавливаем рабочую директорию внутри контейнера
WORKDIR /app

# Копируем исходный код проекта в контейнер
COPY . /app

# Создаем build директорию и выполняем cmake
RUN mkdir build && cd build && cmake ..

# Собираем проект
RUN cd build && make

# Указываем порт, который будет слушать сервер
EXPOSE 5000

# Указываем команду для запуска сервера по умолчанию
CMD ["./server"]





