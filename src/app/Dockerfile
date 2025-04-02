# # Dockerfile для сборки TCP/gRPC мессенджера + интеграция Prometheus
# FROM archlinux:latest

# # Установка базовых зависимостей
# RUN pacman -Syu --noconfirm \
#     base-devel \
#     boost \
#     cmake \
#     git \
#     protobuf \
#     grpc \
#     grpc-cli \
#     nlohmann-json \
#     gnuplot \
#     make \
#     gcc \
#     pkgconf \
#     curl \
#     wget \
#     go \
#     python3



# # Установка prometheus-cpp из исходников
# RUN git clone https://github.com/jupp0r/prometheus-cpp.git && \
#     cd prometheus-cpp && \
#     git submodule update --init && \
#     mkdir build && \
#     cd build && \
#     cmake .. -DBUILD_SHARED_LIBS=ON && \
#     make && \
#     make install

# # Копируем проект
# WORKDIR /app
# COPY . .

# # Сборка проекта
# RUN make clean && make

# # Экспорт портов
# EXPOSE 8080 9090

# # Запуск сервера
# CMD ["./messenger_tcp_server"]


FROM ubuntu:22.04

# Установка зависимостей для CURL и prometheus-cpp
RUN apt update && apt install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libgrpc++-dev \
    nlohmann-json3-dev \
    gnuplot \
    wget \
    libmicrohttpd-dev \
    libcurl4-openssl-dev \
    zlib1g-dev

# Сборка prometheus-cpp
RUN git clone --recursive https://github.com/jupp0r/prometheus-cpp.git && \
    cd prometheus-cpp && \
    mkdir build && \
    cd build && \
    cmake .. -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j1 && \
    make install && \
    ldconfig  

WORKDIR /app
COPY . .

RUN make clean && make

CMD ["./messenger_tcp_server"]