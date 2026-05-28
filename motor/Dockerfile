FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . /app

RUN cmake -S . -B build -DMANCALA_BUILD_SERVER=ON -DCMAKE_CXX_COMPILER=g++ -DCMAKE_CXX_FLAGS="-fopenmp" \
 && cmake --build build -j

FROM ubuntu:22.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    libgomp1 \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/mancala_server /usr/local/bin/mancala_server

EXPOSE 9000
CMD ["/usr/local/bin/mancala_server"]
