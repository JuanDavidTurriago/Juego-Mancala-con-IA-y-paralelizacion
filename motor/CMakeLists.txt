cmake_minimum_required(VERSION 3.20)
project(mancala_motor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MANCALA_BUILD_SERVER "Build C++ HTTP server (mancala_server)" OFF)
option(MANCALA_BUILD_BENCH "Build benchmark binary (mancala_bench)" OFF)
option(MANCALA_BUILD_SEARCH "Build search engines and their tests" OFF)

add_library(mancala
  src/board.cpp
)

target_include_directories(mancala PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(test_board tests/test_board.cpp)
target_link_libraries(test_board PRIVATE mancala)

if(MANCALA_BUILD_SEARCH OR MANCALA_BUILD_BENCH)
  target_sources(mancala PRIVATE
    src/alphabeta.cpp
    src/mcts.cpp
  )
endif()

if(MANCALA_BUILD_SEARCH)
  add_executable(test_alphabeta tests/test_alphabeta.cpp)
  target_link_libraries(test_alphabeta PRIVATE mancala)

  add_executable(test_mcts tests/test_mcts.cpp)
  target_link_libraries(test_mcts PRIVATE mancala)
endif()

if(MANCALA_BUILD_BENCH)
  add_executable(mancala_bench bench/mancala_bench.cpp)
  target_link_libraries(mancala_bench PRIVATE mancala)
endif()

if(MANCALA_BUILD_SERVER)
  add_executable(mancala_server src/server.cpp)
  target_link_libraries(mancala_server PRIVATE mancala)
endif()
