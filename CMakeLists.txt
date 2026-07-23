cmake_minimum_required(VERSION 3.10)

project(cpp-http-server)

set(CMAKE_CXX_STANDARD 17)

add_executable(cpp-http-server
    src/main.cpp
    src/FileSystem.cpp
)

target_include_directories(cpp-http-server PRIVATE include)