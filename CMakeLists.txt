cmake_minimum_required(VERSION 3.0)

project(LowLatencyApp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER g++)
set(GLOBAL_FLAGS -std=c++2a -Wall -Wextra -Werror -Wpedantic)
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_VERBOSE_MAKEFILE on)

add_subdirectory(common)
add_subdirectory(exchange)
add_subdirectory(trading)
add_subdirectory(benchmarks)

list(APPEND LIBS libexchange)
list(APPEND LIBS libcommon)
list(APPEND LIBS libtrading)
list(APPEND LIBS pthread)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/exchange)
include_directories(${PROJECT_SOURCE_DIR}/trading)

add_executable(exchange_main exchange/ExchangeMain.cpp)
target_link_libraries(exchange_main PUBLIC ${LIBS})

add_executable(trading_main trading/TradingMain.cpp)
target_link_libraries(trading_main PUBLIC ${LIBS})

add_executable(logger_benchmark benchmarks/LoggerBenchmark.cpp)
target_link_libraries(logger_benchmark PUBLIC ${LIBS})

add_executable(release_benchmark benchmarks/ReleaseBenchmark.cpp)
target_link_libraries(release_benchmark PUBLIC ${LIBS})

add_executable(hash_benchmark benchmarks/HashBenchmark.cpp)
target_link_libraries(hash_benchmark PUBLIC ${LIBS})
