#include "FileSystem.h"
#include "Router.h"
#include "Server.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

static Request request(const std::string &method, const std::string &path)
{
    Request result;
    result.method = method;
    result.path = path;
    result.version = "HTTP/1.1";
    result.isValid = true;
    return result;
}

static bool hasHeader(const Response &response, const std::string &name, const std::string &value)
{
    return std::find(response.headers.begin(), response.headers.end(),
                     std::make_pair(name, value)) != response.headers.end();
}

TEST_CASE("The router serves the configured index without sockets")
{
    ServerConfig config;
    config.docRoot = TEST_DOC_ROOT;
    const auto response = route(request("GET", "/"), config);
    CHECK(response.status == 200);
    const auto index = readFile(config.docRoot, "index.html");
    REQUIRE(index.success);
    CHECK(response.body == index.content);
    CHECK(hasHeader(response, "Content-Type", "text/html"));
}

TEST_CASE("The router handles health without a document root")
{
    ServerConfig config;
    config.docRoot = std::string(TEST_DOC_ROOT) + "/missing-root";
    const auto response = route(request("GET", "/health"), config);
    CHECK(response.status == 200);
    CHECK(response.body == "ok\n");
    CHECK(hasHeader(response, "Content-Type", "text/plain"));
}

TEST_CASE("The router serves text and binary files without changing bytes")
{
    ServerConfig config;
    config.docRoot = TEST_DOC_ROOT;
    const auto text = route(request("GET", "/hello.txt"), config);
    CHECK(text.status == 200);
    CHECK(text.body == "Hello from the test server!\n");
    CHECK(hasHeader(text, "Content-Type", "text/plain"));
    const auto binary = route(request("GET", "/data.bin"), config);
    CHECK(binary.status == 200);
    REQUIRE(binary.body.size() == 1000);
    CHECK(binary.body == readFile(config.docRoot, "data.bin").content);
    CHECK(hasHeader(binary, "Content-Type", "application/octet-stream"));
}

TEST_CASE("The router handles missing files and rejects traversal")
{
    ServerConfig config;
    config.docRoot = TEST_DOC_ROOT;
    for (const auto *path : {"/nope", "/../../../CMakeLists.txt"})
    {
        INFO(path);
        const auto response = route(request("GET", path), config);
        CHECK(response.status == 404);
        CHECK(response.body == "<h1>Fixture: 404 Not Found</h1>\n");
    }
}

TEST_CASE("Unsupported methods keep their current Allow header")
{
    ServerConfig config;
    config.docRoot = TEST_DOC_ROOT;
    for (const auto *method : {"POST", "HEAD", "OPTIONS", "PUT"})
    {
        INFO(method);
        const auto response = route(request(method, "/"), config);
        CHECK(response.status == 405);
        CHECK(response.body == "<h1>405 Method Not Allowed</h1>");
        CHECK(hasHeader(response, "Allow", "GET"));
        CHECK(hasHeader(response, "Content-Type", "text/html"));
    }
}
