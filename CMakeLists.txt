cmake_minimum_required(VERSION 3.16)

project(cpp-http-server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SANITIZERS_DEFAULT OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(SANITIZERS_DEFAULT ON)
endif()
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" ${SANITIZERS_DEFAULT})

add_library(project_options INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(project_options INTERFACE -Wall -Wextra -Wpedantic)
    if(ENABLE_SANITIZERS)
        target_compile_options(project_options INTERFACE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(project_options INTERFACE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
    endif()
elseif(ENABLE_SANITIZERS)
    message(FATAL_ERROR "ENABLE_SANITIZERS requires GCC or Clang")
endif()

find_package(Threads REQUIRED)

# Everything except main(), so the tests can use the same code.
add_library(http_core STATIC
    src/ClientHandler.cpp
    src/FileSystem.cpp
    src/HttpParser.cpp
    src/HttpResponse.cpp
    src/Router.cpp
    src/Server.cpp
    src/Utils.cpp
)
target_include_directories(http_core PUBLIC include)
target_link_libraries(http_core PUBLIC Threads::Threads project_options)

add_executable(cpp-http-server src/main.cpp)
target_link_libraries(cpp-http-server PRIVATE http_core)

include(CTest)
if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(http_unit_tests
        tests/unit/test_parser.cpp
        tests/unit/test_utils.cpp
        tests/unit/test_filesystem.cpp
        tests/unit/test_response.cpp
        tests/unit/test_router.cpp
        tests/unit/test_server.cpp
    )
    target_link_libraries(http_unit_tests PRIVATE http_core Catch2::Catch2WithMain)
    target_compile_definitions(http_unit_tests PRIVATE
        TEST_DOC_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/www")
    add_test(NAME http_unit_tests COMMAND http_unit_tests)

    add_executable(http_integration_tests
        tests/integration/test_basic.cpp
        tests/integration/test_server.cpp
    )
    target_link_libraries(http_integration_tests PRIVATE http_core Catch2::Catch2WithMain)
    target_compile_definitions(http_integration_tests PRIVATE
        TEST_DOC_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/www")
    add_dependencies(http_integration_tests cpp-http-server)
    add_test(NAME http_integration_tests COMMAND http_integration_tests)
    set_tests_properties(http_unit_tests PROPERTIES LABELS unit TIMEOUT 30)
    set_tests_properties(http_integration_tests PROPERTIES LABELS integration TIMEOUT 30)
endif()
