# Phase 1: A Correct HTTP/1.1 Server

**Goal:** turn the current "one request, then close" static file server into a
standards-compliant HTTP/1.1 server with persistent connections, request bodies
(including chunked), HEAD, conditional GET and byte ranges, all covered by
automated tests that talk to a real running server.

**Not in Phase 1 (do not start these yet):** thread pool, epoll/io_uring,
`sendfile()`, TLS, router with path params, gzip, HTTP/2, multipart ranges,
benchmarks. The architecture stays thread-per-connection for now.

**How to work through it:** one milestone = one branch = one or more commits.
Do not start a milestone until the previous one's "Done when" boxes are all
ticked. Run every test under the sanitizers (M0) from the beginning.

| # | Milestone | Rough effort |
|---|-----------|--------------|
| M0 | Project setup | 30 min |
| M1 | Test infrastructure | 1–2 days |
| M2 | Response/routing refactor | 1 day |
| M3 | Strict parser | 1–2 days |
| M4 | Keep-alive and pipelining | 1–2 days |
| M5 | HEAD, POST, chunked bodies, 100-continue | 2 days |
| M6 | Date, ETag/304, Range/206 | 1–2 days |
| M7 | Release v0.2.0 | 2 hours |

---

> Verification status (2026-11-25): the user ran the Debug configure, build,
> and CTest commands locally. Both `http_unit_tests` and
> `http_integration_tests` passed (2/2 CTest targets), with no build warnings or
> sanitizer reports in the supplied output. `ENABLE_SANITIZERS=ON` is confirmed
> in the build cache. M1 completion checks pass; Git branch/commit steps remain
> pending because this assistant session cannot write Git metadata.

## M0: Project setup

- [ ] Branch `phase1/m0-setup` from `main`. (Blocked: Git metadata is read-only.)
- [x] Add a CMake option `ENABLE_SANITIZERS` (default ON for Debug) that adds
      `-fsanitize=address,undefined -fno-omit-frame-pointer` to compile and link
      flags for `http_core`, the server, and the tests.
- [x] Add `-Wall -Wextra -Wpedantic` to every target (only `http_core` has it today).
- [ ] Add a `.clang-format` file and format the code once in a separate commit.
      (File added and code formatted; separate commit blocked by read-only Git metadata.)

**Done when:**
- [x] `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ctest --test-dir build` passes with sanitizers on and no warnings.
      (Verified from the user's local run on 2026-11-25; sanitizers enabled in the build cache.)

---

## M1: Test infrastructure

Everything after this milestone is tested against a live server, so this comes first.

### 1.1 Test framework
- [x] Pull in **Catch2 v3** with `FetchContent` in `CMakeLists.txt`.
- [x] Move the existing tests from `tests/tests.cpp` into:
  - `tests/unit/test_parser.cpp`
  - `tests/unit/test_utils.cpp`
  - `tests/unit/test_filesystem.cpp`
- [x] Delete the hand-written `CHECK` macro.

### 1.2 Extract a `Server` class from `main.cpp`
New files: `include/Server.h`, `src/Server.cpp`.

```cpp
struct ServerConfig {
    std::string host = "127.0.0.1";
    int port = 8080;               // 0 = let the OS pick (used by tests)
    std::string docRoot = "public";
};

class Server {
public:
    explicit Server(ServerConfig config);
    bool start();       // socket + setsockopt + bind + listen; false on failure
    int port() const;   // real port after bind (use getsockname)
    void run();         // accept loop; returns after stop()
    void stop();        // shutdown(listenFd, SHUT_RDWR) so accept() returns
};
```
- [x] `main.cpp` only parses arguments (`[port] [docRoot]`) and calls `start()` + `run()`.
- [x] Handle SIGINT/SIGTERM in `main`: call `stop()` and exit cleanly.

### 1.3 Configurable document root
- [x] Change `readFile(path)` to `readFile(docRoot, path)`. Remove the hard-coded
      `"public"` in `isInsidePublic` (rename it `isInsideRoot`).
- [x] Pass `docRoot` from `ServerConfig` through `handleClient`.
- [x] Create `tests/fixtures/www/` with `index.html`, `hello.txt` (known exact
      contents), `data.bin` (exactly 1000 bytes), and `404.html`.

### 1.4 Integration test harness
New file: `tests/integration/harness.h`.
- [x] `class TestServer`: an RAII fixture. The constructor starts a `Server` on
      port 0 with docRoot `tests/fixtures/www` in a `std::thread`. The destructor
      calls `stop()` and joins.
- [x] `std::string sendRaw(int port, const std::string& bytes)`: connect, send the
      bytes exactly as given, read until the server closes the connection (or a
      2 s timeout), and return everything received.
- [x] `struct ParsedResponse { int status; map headers; std::string body; }`, plus
      `std::vector<ParsedResponse> parseResponses(const std::string&)`, which splits
      several responses using `Content-Length`. M4 needs this for pipelining tests.

**Done when:**
- [x] `tests/integration/test_basic.cpp` passes:
  - `GET / HTTP/1.1` → 200, body equals `index.html`
  - `GET /hello.txt` → 200, `Content-Type: text/plain`, exact body
  - `GET /nope` → 404
  - `GET /../CMakeLists.txt` → 404 (traversal still blocked)
  - `GET /health` → 200, body `ok\n`
- [x] All unit tests ported and passing.
- [x] `ctest` runs the unit and integration tests as two separate test targets.

---

## M2: Response and routing refactor

Implementation and unit tests verified on 2026-11-25: warning-free Debug build,
ASan/UBSan unit tests pass with `ASAN_OPTIONS=detect_leaks=0` (this session
prevents LeakSanitizer). The user's subsequent local CTest run passed both
targets (2/2), including the unchanged M1 integration tests, with no sanitizer
errors reported. M2 completion checks pass. The milestone's Git branch/commits
remain pending; this session cannot write Git metadata.

### 2.1 `Response` type (replace `buildResponse`)
In `include/HttpResponse.h`:
```cpp
struct Response {
    int status = 200;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    bool omitBody = false;          // HEAD and 304 (used in M5/M6)
};
std::string_view reasonPhrase(int status);
std::string serialize(const Response&, bool keepAlive);
```
- [x] `reasonPhrase` covers: 100, 200, 204, 206, 304, 400, 404, 405, 408, 411,
      413, 414, 416, 417, 431, 500, 501, 505.
- [x] `serialize` always adds `Content-Length`, `Connection` (`keep-alive`/`close`),
      `Server: cpp-http-server/0.2`, and (from M6) `Date`.
- [x] Helper: `Response errorResponse(int status, const ServerConfig&)` uses
      `<status>.html` from docRoot if it exists, otherwise `<h1>404 Not Found</h1>`.

### 2.2 Router
New files: `include/Router.h`, `src/Router.cpp`.
- [x] `Response route(const Request&, const ServerConfig&)`. Move all the
      `if (method == ...)` / file-serving logic out of `ClientHandler.cpp` into here.
- [x] `handleClient` only does: read → parse → `route()` → `serialize()` → send.

**Done when:**
- [x] Unit tests for `serialize()` check the exact output bytes.
- [x] Unit tests call `route()` directly (no sockets) for `/`, `/health`, 404 and 405.
- [x] All M1 integration tests still pass, unchanged.
      (Verified from the user's local CTest output on 2026-11-25.)

---

## M3: Strict request parser

### 3.1 New parser API
- [ ] Rewrite `HttpParser.cpp` on `std::string_view` with no `stringstream`.
      Remove `<sstream>` from `HttpParser.h`.
- [ ] Return the reason for failure, not only `isValid`:
```cpp
struct ParseResult {
    Request request;
    int errorStatus = 0;   // 0 = OK, otherwise 400 / 414 / 431 / 501 / 505
};
ParseResult parseRequestHead(std::string_view head);  // request line + headers
```

### 3.2 Rules to implement. Each row is one test.

| Input | Expected |
|---|---|
| `GET / HTTP/1.1` + `Host` | 200 |
| `GET  / HTTP/1.1` (double space) | 400 |
| `GET / HTTP/1.1 extra` | 400 |
| `GET /` (no version) | 400 |
| `GET / HTTP/2.0` or `HTTP/1.2` | 505 |
| `GET / FTP/1.0` | 400 |
| Method with non-token characters (`GE(T`) | 400 |
| Method `BREW` (valid token, unknown) | 501 |
| Method `PUT` / `DELETE` / `PATCH` (known, unsupported) | 405 + `Allow: GET, HEAD, POST, OPTIONS` |
| Request target > 8192 bytes | 414 |
| HTTP/1.1 with no `Host` | 400 |
| Two `Host` headers | 400 |
| HTTP/1.0 with no `Host` | allowed |
| `Host : x` (space before colon) | 400 |
| Header name with non-token characters | 400 |
| Header line starting with space/tab (obs-fold) | 400 |
| Header value containing a NUL, bare CR, or other control character (except tab) | 400 |
| More than 100 header lines | 431 |
| Header block > 8 KB | 431 (already exists, keep it) |
| Lines ending in bare `\n` instead of `\r\n` | 400 |
| `Content-Length: abc` / `5, 10` / negative | 400 |
| `Content-Length` **and** `Transfer-Encoding` | 400, then close |
| `Transfer-Encoding: gzip` (anything other than exactly `chunked`) | 501 |
| POST with no `Content-Length` and no `Transfer-Encoding` | 411 Length Required |

- [ ] Use a "token" character helper: `!#$%&'*+-.^_`|~`, digits and letters.
- [ ] Keep the existing query-string and percent-decoding behaviour and tests.

**Done when:**
- [ ] Every row above is a unit test on `parseRequestHead` **and** at least 8 of
      them are also integration tests over a socket.
- [ ] The server sends the matching status code (not just 400) for each.

---

## M4: Persistent connections and pipelining

### 4.1 `Connection` class
New files: `include/Connection.h`, `src/Connection.cpp`. It replaces the body of `handleClient`.
```cpp
class Connection {
    int fd;
    std::string buffer;          // survives between requests
    int requestsServed = 0;
public:
    void serve();                // the loop below
};
```

### 4.2 The loop
```
while true:
    wait for data with poll(fd, IDLE_TIMEOUT) if buffer is empty → timeout: close quietly
    read until "\r\n\r\n" in buffer (REQUEST_TIMEOUT) → timeout: 408 + close
    parseRequestHead → error: send error, close
    read body (Content-Length or chunked from M5)
    buffer.erase(0, bytesConsumedByThisRequest)   ← never discard pipelined bytes
    response = route(request)
    keepAlive = decideKeepAlive(request, response)
    send serialize(response, keepAlive)
    if !keepAlive: break
close(fd)
```

### 4.3 Exact rules
- [ ] Constants: `IDLE_TIMEOUT = 5s`, `REQUEST_TIMEOUT = 10s`,
      `MAX_REQUESTS_PER_CONNECTION = 100`.
- [ ] `decideKeepAlive`:
  - HTTP/1.1: keep alive unless the request has `Connection: close`.
  - HTTP/1.0: close unless the request has `Connection: keep-alive`.
  - Always close after: any 4xx/5xx produced by the parser, 408, a broken
    body, or reaching `MAX_REQUESTS_PER_CONNECTION`.
- [ ] Parse the `Connection` header as a comma-separated, case-insensitive list
      (`Connection: Keep-Alive, Upgrade` counts as keep-alive).
- [ ] Response sends `Connection: keep-alive` or `Connection: close` to match.

**Done when these integration tests pass:**
- [ ] Two sequential requests on one socket → two 200 responses, same connection.
- [ ] Two requests in **one `send()`** (pipelined) → two responses, in order.
- [ ] Three pipelined requests where the 2nd has a body → all three correct.
- [ ] `Connection: close` → response has `Connection: close`, then the server closes.
- [ ] HTTP/1.0 without keep-alive → closed after one response.
- [ ] HTTP/1.0 with `Connection: keep-alive` → stays open.
- [ ] Idle for longer than `IDLE_TIMEOUT` → server closes without sending anything.
- [ ] Half a request, then silence → 408.
- [ ] 101st request on one connection → served with `Connection: close`.
- [ ] Malformed 2nd pipelined request → 1st gets 200, 2nd gets 400, then close.

---

## M5: More methods and request bodies

### 5.1 HEAD
- [ ] HEAD runs exactly the same route as GET, then sets `omitBody = true`.
- [ ] `Content-Length` = the length the GET body **would** have had.
- [ ] Test: `HEAD /hello.txt` → same headers as GET, zero body bytes, and the
      connection is still usable for a following GET.

### 5.2 POST
- [ ] `POST /echo` → 200, body = request body, `Content-Type` copied from the
      request (default `application/octet-stream`).
- [ ] POST to any other path → 405 with an `Allow` header.

### 5.3 OPTIONS
- [ ] `OPTIONS <any path>` → 204 with `Allow: GET, HEAD, POST, OPTIONS`.
- [ ] `OPTIONS *` is accepted as a valid request target for OPTIONS only.

### 5.4 Chunked request bodies
New files: `include/ChunkedDecoder.h`, `src/ChunkedDecoder.cpp`.
```cpp
enum class ChunkStatus { NeedMore, Done, Error, TooLarge };
// Decodes from buffer starting at `pos`. On Done, `pos` points just past the
// final CRLF and `out` holds the decoded body.
ChunkStatus decodeChunked(std::string_view buffer, size_t& pos,
                          std::string& out, size_t maxBody);
```
- [ ] Grammar: `hex-size [;ext...] CRLF data CRLF` repeated, ending with
      `0 CRLF [trailer-lines] CRLF`.
- [ ] Hex is case-insensitive, at most 16 digits, with overflow checked → Error.
- [ ] Chunk extensions are ignored. Trailer lines are read and discarded.
- [ ] Data not followed by CRLF → Error.
- [ ] Total decoded size > `MAX_BODY_BYTES` → TooLarge → 413.
- [ ] It must be **incremental**: calling it on a partial buffer returns NeedMore,
      and calling again after more bytes arrive continues correctly.

### 5.5 `Expect: 100-continue`
- [ ] If the request has `Expect: 100-continue` and the body is acceptable: send
      `HTTP/1.1 100 Continue\r\n\r\n` before reading the body.
- [ ] If `Content-Length` > max: send 413 immediately without reading the body, then close.
- [ ] Any other `Expect` value → 417.

**Done when:**
- [ ] Unit tests for `decodeChunked`: single chunk, many chunks, uppercase hex,
      extensions, trailers, a body split at **every possible byte position**
      (loop over split points), bad hex, missing CRLF, overflowing size, too large.
- [ ] Integration: chunked `POST /echo` returns the decoded body. A chunked POST
      followed by a pipelined GET works (proves the body boundary is correct).
- [ ] Integration: `curl -X POST --data-binary @file -H "Expect: 100-continue"` works.

---

## M6: Dates, conditional GET and ranges

### 6.1 File metadata
- [ ] Extend `FileResult` with `size` (`uint64_t`) and `mtime` (`time_t`), from `stat()`.

### 6.2 HTTP dates
- [ ] `std::string formatHttpDate(time_t)` → `Sun, 06 Nov 1994 08:49:37 GMT`
      (use `gmtime_r` + `strftime`, **not** the locale).
- [ ] `std::optional<time_t> parseHttpDate(std::string_view)`: IMF-fixdate format
      only; anything else → `nullopt` (which means the header is ignored).
- [ ] Every response gets a `Date` header.

### 6.3 Validators
- [ ] `Last-Modified: formatHttpDate(mtime)` on file responses.
- [ ] `ETag: "<mtime-hex>-<size-hex>"` (strong) on file responses.

### 6.4 Conditional GET (GET and HEAD only, only when the result would be 200)
- [ ] `If-None-Match` matches the ETag (also supports `*` and comma lists) → **304**.
- [ ] Otherwise, if there is **no** `If-None-Match` and `If-Modified-Since` ≥ mtime → **304**.
- [ ] 304 has no body and includes `ETag`, `Last-Modified` and `Date`.

### 6.5 Range requests (GET only, on files)
- [ ] All file responses include `Accept-Ranges: bytes`.
- [ ] Supported forms: `bytes=0-99`, `bytes=900-`, `bytes=-100`.
- [ ] Valid → **206** with `Content-Range: bytes 0-99/1000` and only those bytes.
- [ ] End past the file size → clamp to `size-1`.
- [ ] Start ≥ size → **416** with `Content-Range: bytes */1000`.
- [ ] Multiple ranges (`bytes=0-1,5-6`) or a syntax error → ignore the header, send 200.
- [ ] `If-Range` matching the ETag → honour the Range; not matching → send 200.

**Done when (all use `data.bin`, 1000 bytes):**
- [ ] Unit tests: `formatHttpDate`/`parseHttpDate` round-trip, plus invalid dates.
- [ ] GET → `ETag`, then GET with `If-None-Match: <that etag>` → 304, empty body.
- [ ] `If-None-Match: "wrong"` → 200.
- [ ] `If-Modified-Since: <future date>` → 304. `<1990 date>` → 200.
- [ ] `Range: bytes=0-99` → 206, 100 bytes, correct `Content-Range`.
- [ ] `bytes=-100` → last 100 bytes. `bytes=900-` → last 100 bytes.
- [ ] `bytes=990-2000` → 206, 10 bytes. `bytes=1000-` → 416.
- [ ] `bytes=0-1,5-6` → 200, full file.
- [ ] HEAD with Range → 206 headers, no body.

---

## M7: Release v0.2.0

- [ ] README: a "Supported HTTP features" table (methods, keep-alive, chunked,
      conditional, ranges, limits and timeouts with their values).
- [ ] README: `curl` examples for each feature (keep-alive with `curl -v url url`,
      range with `-r 0-99`, conditional with `-H 'If-None-Match: ...'`).
- [ ] README: a short "Architecture" section (Server → Connection → Parser → Router → Response).
- [ ] Full test suite green under ASan/UBSan.
- [ ] Merge to `main`, tag `v0.2.0`.

## Phase 1 definition of done

- [ ] Every checkbox above is ticked.
- [ ] No test needs a manual step. `ctest` runs everything.
- [ ] No sanitizer reports.
- [ ] You can explain, without notes: why pipelined bytes must not be discarded,
      why CL + TE is rejected (request smuggling), why HEAD keeps
      `Content-Length`, and why `If-None-Match` takes precedence over `If-Modified-Since`.
