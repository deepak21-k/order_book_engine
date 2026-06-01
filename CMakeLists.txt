cmake_minimum_required(VERSION 3.14)
project(order_book_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Compiler warnings ──────────────────────────────────────────────────────
add_compile_options(-Wall -Wextra -Wpedantic)

# ── Release build optimisations ───────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ── Main executable ────────────────────────────────────────────────────────
add_executable(order_book_engine
    src/main.cpp
    src/OrderBook.cpp
)

target_include_directories(order_book_engine PRIVATE src)

# ── Tests ──────────────────────────────────────────────────────────────────
enable_testing()

add_executable(run_tests
    tests/test_orderbook.cpp
    src/OrderBook.cpp
)

target_include_directories(run_tests PRIVATE src)

add_test(NAME OrderBookTests COMMAND run_tests)
