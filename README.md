# cpp-http-server

A small HTTP server built from scratch in C++ using POSIX sockets.

## Build and run

```bash
cmake -S . -B build
cmake --build build
./build/cpp-http-server            # listens on 127.0.0.1:8080
./build/cpp-http-server 9000      # choose a different port
./build/cpp-http-server 9000 /path/to/site  # choose a document root
```

Run the tests from the repository root:

```bash
ctest --test-dir build --output-on-failure
```

The server serves files from `public/` and includes a simple health check at
`GET /health`. It currently supports static `GET` requests, common MIME types,
query-string parsing, and custom 404/405 pages.

The server handles one request per connection. SIGINT (Ctrl+C) and SIGTERM
stop the listener and active client connections, then join the client threads.

## Development and tests

Use a Debug build to enable AddressSanitizer and UndefinedBehaviorSanitizer by
default. GCC and Clang builds also enable `-Wall -Wextra -Wpedantic` on all
project targets.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

`ENABLE_SANITIZERS` can be set explicitly to `ON` or `OFF`. Its default is chosen
when a build directory is first configured; an existing CMake cache preserves
the previous choice.

The first test-enabled configure downloads Catch2 v3.7.1 from its official
repository using FetchContent. To use an already downloaded copy offline, add
`-DFETCHCONTENT_SOURCE_DIR_CATCH2=/absolute/path/to/Catch2` when configuring.
To build only the server without downloading test dependencies, use a separate
build directory with `-DBUILD_TESTING=OFF`.

CTest runs two targets:

| Target | Coverage |
| --- | --- |
| `http_unit_tests` | Request parsing, string helpers, file access, exact response serialization, direct routing, invalid server configuration |
| `http_integration_tests` | Live HTTP responses, exact binary contents, traversal rejection, concurrent clients, shutdown, response splitting, socket setup |

Integration tests require permission to create and bind local TCP sockets. They
use OS-assigned ports and an absolute fixture document root, so they can run
from the build directory without changing the process's working directory.

Some restricted debugging environments prevent LeakSanitizer from inspecting
threads. Only in those environments, run tests with
`ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build --output-on-failure`.
AddressSanitizer and UndefinedBehaviorSanitizer remain enabled; leak detection
is disabled for that invocation.

## Request handling

`Server` accepts connections and passes its configuration to `ClientHandler`.
The handler reads and parses a request, calls `route(request, config)`, then
sends `serialize(response, false)`. Connections still close after one request.

`Router` selects the static file, health response, or error response. The
`Response` structure holds a status code, headers, body, and an `omitBody` flag.
`HttpResponse` serializes those fields with one authoritative `Content-Length`,
`Connection`, and `Server` header. It can omit the body while preserving its
length; HEAD support will use that in a later milestone.

Error pages are loaded from the configured document root. If a page is missing,
the fallback HTML includes the requested status and its reason phrase.

Format C++ code with the repository's `.clang-format`, for example:

```bash
clang-format -i include/*.h src/*.cpp tests/unit/*.cpp tests/integration/*.h tests/integration/*.cpp
```
