cmake_minimum_required(VERSION 3.15)
project(OrderBook VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options(-march=native -mtune=native)
    # Enable SIMD intrinsics
    add_compile_options(-mavx2 -mfma)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Shared library sources (everything except main.cpp)
set(LIB_SOURCES
    src/order.cpp
    src/tick_data.cpp
    src/order_book.cpp
    src/market_microstructure_analyzer.cpp
    src/execution_strategy.cpp
    src/twap_strategy.cpp
    src/vwap_strategy.cpp
    src/almgren_chriss_strategy.cpp
    src/backtester.cpp
    src/matching_engine.cpp
)

# Main executable
add_executable(orderbook ${LIB_SOURCES} src/main.cpp)

# Test executable
add_executable(orderbook_tests ${LIB_SOURCES} tests/test_main.cpp)

# Optional: Link against threading library
find_package(Threads REQUIRED)
target_link_libraries(orderbook       Threads::Threads)
target_link_libraries(orderbook_tests Threads::Threads)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Release optimizations
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# CTest integration
enable_testing()
add_test(NAME unit_tests COMMAND orderbook_tests)
