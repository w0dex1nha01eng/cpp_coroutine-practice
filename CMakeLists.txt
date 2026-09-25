cmake_minimum_required(VERSION 3.20)

project(cpp20_coroutine_async_comm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(async_comm
    src/event_loop.cpp
    src/socket.cpp
)

target_include_directories(async_comm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(async_comm PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(echo_server achieve/echo_server.cpp)
target_link_libraries(echo_server PRIVATE async_comm)

add_executable(echo_client achieve/echo_client.cpp)
target_link_libraries(echo_client PRIVATE async_comm)

if(WIN32)
    target_link_libraries(async_comm PUBLIC ws2_32)
endif()
