# Dockerfile для сборки TCP/gRPC мессенджера + бенчмарков
FROM archlinux:latest

# Установка зависимостей
RUN pacman -Syu --noconfirm \
    base-devel \
    boost \
    cmake \
    git \
    protobuf \
    grpc \
    grpc-cli \
    nlohmann-json \
    gnuplot \
    make \
    gcc \
    pkgconf

# Копируем весь проект внутрь контейнера
WORKDIR /app
COPY . /app

# Генерация .pb файлов, сборка проекта
RUN make clean && make

# Устанавливаем точку входа (можно изменить на нужный исполняемый файл)
CMD ["./messenger_tcp_server"]