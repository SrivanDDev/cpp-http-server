cmake_minimum_required(VERSION 3.10)

project(cpp-http-server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

# Everything except main(), so the tests can use the same code.
add_library(http_core STATIC
    src/ClientHandler.cpp
    src/FileSystem.cpp
    src/HttpParser.cpp
    src/HttpResponse.cpp
    src/Utils.cpp
)
target_include_directories(http_core PUBLIC include)
target_link_libraries(http_core PUBLIC Threads::Threads)
target_compile_options(http_core PRIVATE -Wall -Wextra)

add_executable(cpp-http-server src/main.cpp)
target_link_libraries(cpp-http-server PRIVATE http_core)

enable_testing()
add_executable(http_tests tests/tests.cpp)
target_link_libraries(http_tests PRIVATE http_core)
# readFile() looks for public/ in the current directory, so run tests from the repo root.
add_test(NAME http_tests COMMAND http_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
