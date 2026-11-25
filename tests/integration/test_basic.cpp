#include "FileSystem.h"
#include "harness.h"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <future>
#include <iterator>

static std::string fixture(const std::string &path)
{
    std::ifstream file(std::string(TEST_DOC_ROOT) + '/' + path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Missing test fixture: " + path);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

static ParsedResponse get(TestServer &server, const std::string &path)
{
    auto responses = parseResponses(
        sendRaw(server.port(), "GET " + path + " HTTP/1.1\r\nHost: localhost\r\n\r\n"));
    REQUIRE(responses.size() == 1);
    return responses.front();
}

TEST_CASE("GET serves the configured index")
{
    TestServer server;
    auto response = get(server, "/");
    CHECK(response.status == 200);
    CHECK(response.body == fixture("index.html"));
    CHECK(response.headers.at("connection") == "close");
}

TEST_CASE("GET serves text with the right MIME type and exact bytes")
{
    TestServer server;
    auto response = get(server, "/hello.txt");
    CHECK(response.status == 200);
    CHECK(response.headers.at("content-type") == "text/plain");
    CHECK(response.body == fixture("hello.txt"));
}

TEST_CASE("Missing files and traversal use the configured error page")
{
    TestServer server;
    for (const auto *path :
         {"/nope", "/../CMakeLists.txt", "/%2e%2e/CMakeLists.txt", "/../../../CMakeLists.txt"})
    {
        INFO(path);
        auto response = get(server, path);
        CHECK(response.status == 404);
        CHECK(response.body == fixture("404.html"));
    }
}

TEST_CASE("GET health is independent of static files")
{
    TestServer server;
    auto response = get(server, "/health");
    CHECK(response.status == 200);
    CHECK(response.body == "ok\n");
}

TEST_CASE("Binary files are delivered without altering bytes")
{
    TestServer server;
    auto response = get(server, "/data.bin");
    REQUIRE(response.status == 200);
    REQUIRE(response.body.size() == 1000);
    CHECK(response.body == fixture("data.bin"));
    CHECK(response.headers.at("content-length") == "1000");
}

TEST_CASE("Existing method rejection and malformed request responses are preserved")
{
    TestServer server;
    auto method = parseResponses(
        sendRaw(server.port(), "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n"));
    REQUIRE(method.size() == 1);
    CHECK(method.front().status == 405);
    CHECK(method.front().headers.at("allow") == "GET");
    auto bad = parseResponses(sendRaw(server.port(), "GET /\r\n\r\n"));
    REQUIRE(bad.size() == 1);
    CHECK(bad.front().status == 400);
}

TEST_CASE("A slow client does not block another connection")
{
    TestServer server;
    TestSocket slow(server.port());
    slow.sendBytes("GET / HTTP/1.1\r\nHost:");
    CHECK(get(server, "/health").body == "ok\n");
    server.stop();
}

TEST_CASE("Shutdown interrupts incomplete client requests")
{
    // Joining the run thread also joins its client workers. This must not wait
    // for the ten-second receive timeout when stop() is called.
    auto shutdown =
        std::async(std::launch::async,
                   []
                   {
                       TestServer server;
                       TestSocket slow(server.port());
                       slow.sendBytes("GET / HTTP/1.1\r\nHost:");
                       get(server, "/health"); // Prove the accept loop has processed connections.
                       server.stop();
                   });
    CHECK(shutdown.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    shutdown.get();
}

TEST_CASE("Independent servers can run with different document roots")
{
    TestServer files;
    TestServer missing(std::string(TEST_DOC_ROOT) + "/missing-root");
    auto notFound = get(missing, "/hello.txt");
    CHECK(notFound.status == 404);
    CHECK(notFound.body == "<h1>404 Not Found</h1>");
    CHECK(get(files, "/hello.txt").body == fixture("hello.txt"));
}

TEST_CASE("The response harness preserves pipelined response boundaries")
{
    // The server starts serving pipelines in M4. Test the harness itself now.
    const std::string binary("a\0b", 3);
    auto responses = parseResponses("HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\n" + binary +
                                    "HTTP/1.1 404 Not Found\r\nContent-Length: 2\r\n\r\nno");
    REQUIRE(responses.size() == 2);
    CHECK(responses[0].status == 200);
    CHECK(responses[0].body == binary);
    CHECK(responses[1].status == 404);
    CHECK(responses[1].body == "no");
    CHECK_THROWS(parseResponses("HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nx"));
    CHECK_THROWS(parseResponses("HTTP/1.1 200 OK\r\n\r\n"));
}
