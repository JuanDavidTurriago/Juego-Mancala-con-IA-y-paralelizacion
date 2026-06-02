cmake_minimum_required(VERSION 3.16)
project(mancala_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenMP REQUIRED)

add_library(mancala_core
    src/board.cpp
    src/alphabeta.cpp
)
target_include_directories(mancala_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(mancala_core PUBLIC OpenMP::OpenMP_CXX)

add_executable(mancala_bench bench/bench.cpp)
target_link_libraries(mancala_bench PRIVATE mancala_core)

if(NOT WIN32)
    add_executable(mancala_engine src/server.cpp)
    target_link_libraries(mancala_engine PRIVATE mancala_core)
endif()

enable_testing()

add_executable(test_board tests/test_board.cpp)
target_link_libraries(test_board PRIVATE mancala_core)
add_test(NAME board_rules COMMAND test_board)

add_executable(test_alphabeta tests/test_alphabeta.cpp)
target_link_libraries(test_alphabeta PRIVATE mancala_core)
add_test(NAME alphabeta_search COMMAND test_alphabeta)
