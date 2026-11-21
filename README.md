# cpp-http-server

A small HTTP server built from scratch in C++ using POSIX sockets.

## Build and run

```bash
cmake -S . -B build
cmake --build build
./build/cpp-http-server            # listens on 127.0.0.1:8080
./build/cpp-http-server 9000      # choose a different port
```

Run the tests from the repository root:

```bash
ctest --test-dir build --output-on-failure
```

The server serves files from `public/` and includes a simple health check at
`GET /health`. It currently supports static `GET` requests, common MIME types,
query-string parsing, and custom 404/405 pages.
