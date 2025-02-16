# Базовый образ с поддержкой pacman и необходимых библиотек
FROM archlinux:latest

# Устанавливаем зависимости
RUN pacman -Syu --noconfirm \
    gcc \
    cmake \
    make \
    libboost-system \
    libsqlite3 \
    libpthread-stubs \
    libboost-filesystem \
    libboost-program-options \
    libboost-thread \
    libboost-regex \
    pkgconf \
    && rm -rf /var/cache/pacman/pkgcache && pacman -S --noconfirm --needed

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


