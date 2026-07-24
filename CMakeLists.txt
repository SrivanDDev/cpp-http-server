cmake_minimum_required(VERSION 3.10)

project(cpp-http-server)

set(CMAKE_CXX_STANDARD 17)

add_executable(cpp-http-server
    src/ClientHandler.cpp
    src/FileSystem.cpp
    src/HttpParser.cpp
    src/HttpResponse.cpp
    src/main.cpp
    src/Utils.cpp
)

target_include_directories(cpp-http-server PRIVATE include)