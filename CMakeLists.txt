cmake_minimum_required(VERSION 3.10)
project(cpp_redis_server)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_subdirectory(client)

add_executable(server
        src/main.cpp
        src/server.cpp
)
