FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    g++ \
    libgomp1 \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt ./
COPY src ./src
COPY tests ./tests
COPY bench ./bench

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j \
 && ctest --test-dir build --output-on-failure

FROM ubuntu:24.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    libgomp1 \
 && rm -rf /var/lib/apt/lists/* \
 && useradd -r -u 1001 motor

WORKDIR /app
COPY --from=builder /src/build/motor_server /usr/local/bin/motor_server
COPY --from=builder /src/build/bench /usr/local/bin/bench
COPY tests/suite.txt /app/tests/suite.txt

ENV OMP_NUM_THREADS=4
EXPOSE 8080

USER motor
CMD ["/usr/local/bin/motor_server"]
