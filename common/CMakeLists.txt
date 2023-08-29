set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_FLAGS "-std=c++2a -Wall -Wextra -Werror -Wpedantic")
set(CMAKE_VERBOSE_MAKEFILE on)

file(GLOB SOURCES "*.cpp")

include_directories(${PROJECT_SOURCE_DIR})

add_library(libcommon STATIC ${SOURCES})

list(APPEND LIBS libcommon)
list(APPEND LIBS pthread)

add_executable(thread_example ThreadExample.cpp)
target_link_libraries(thread_example PUBLIC ${LIBS})

add_executable(memory_pool_example MemoryPoolExample.cpp)
target_link_libraries(memory_pool_example PUBLIC ${LIBS})

add_executable(lock_free_queue_example LockFreeQueueExample.cpp)
target_link_libraries(lock_free_queue_example PUBLIC ${LIBS})

add_executable(logging_example LoggingExample.cpp)
target_link_libraries(logging_example PUBLIC ${LIBS})

add_executable(socket_example SocketExample.cpp)
target_link_libraries(socket_example PUBLIC ${LIBS})
