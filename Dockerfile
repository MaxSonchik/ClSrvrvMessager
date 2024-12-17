# Используем официальный образ Ubuntu в качестве базового
FROM ubuntu:20.04

# Отключаем взаимодействие с пользователем при установках (tzdata и т.д.)
ENV DEBIAN_FRONTEND=noninteractive

# Обновляем пакеты и устанавливаем необходимые инструменты для сборки и запуск проекта
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libboost-all-dev \
    libsqlite3-dev \
    sqlite3 \
    git \
    && rm -rf /var/lib/apt/lists/*

# Создаём директорию для нашего проекта
WORKDIR /app

# Копируем файлы проекта в контейнер
COPY . /app

# Выполняем сборку проекта
RUN make clean && make

# Предполагается, что в результате сборки у нас есть бинарные файлы server и client
# Создадим директорию для базы данных
RUN mkdir /app/data

# Открываем порт для сервера, например 5000 (как указано в проекте)
EXPOSE 5000

# По умолчанию запускаем сервер, передадим путь к базе данных:
CMD ["./server", "/app/data/database.db"]
