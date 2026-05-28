FROM ubuntu:22.04 AS builder

# g++ provides the OpenMP build support for -fopenmp; libgomp1 provides the GCC OpenMP runtime.
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake g++ libgomp1 \
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

ENV OMP_NUM_THREADS=4
EXPOSE 8080
CMD ["/usr/local/bin/mancala_server"]
