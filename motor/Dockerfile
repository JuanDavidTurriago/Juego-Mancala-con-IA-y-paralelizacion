FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . /app

RUN cmake -S . -B build -DMANCALA_BUILD_SERVER=ON \
 && cmake --build build -j

EXPOSE 9000
CMD ["./build/mancala_server"]
