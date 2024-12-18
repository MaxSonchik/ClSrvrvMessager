# Базовый образ с поддержкой g++ и необходимых библиотек
FROM ubuntu:22.04

# Обновляем систему и устанавливаем необходимые зависимости
RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    make \
    libboost-system-dev \
    libsqlite3-dev \
    libpthread-stubs0-dev \
    pkg-config \
    && apt-get clean

# Устанавливаем рабочую директорию внутри контейнера
WORKDIR /app

# Копируем исходный код проекта в контейнер
COPY . /app

# Собираем проект
RUN make clean && make

# Указываем порт, который будет слушать сервер
EXPOSE 5000

# Указываем команду для запуска сервера по умолчанию
CMD ["./server"]
