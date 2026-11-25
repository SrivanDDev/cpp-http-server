#include "HttpResponse.h"
#include "Server.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Serialization produces exact HTTP bytes")
{
    Response response;
    response.status = 405;
    response.headers = {{"Content-Type", "text/html"}, {"Allow", "GET"}};
    response.body = "hi";
    CHECK(serialize(response, false) == "HTTP/1.1 405 Method Not Allowed\r\n"
                                        "Content-Type: text/html\r\n"
                                        "Allow: GET\r\n"
                                        "Content-Length: 2\r\n"
                                        "Connection: close\r\n"
                                        "Server: cpp-http-server/0.2\r\n"
                                        "\r\nhi");
}

TEST_CASE("Serialization preserves binary bodies and connection choice")
{
    Response response;
    response.body = std::string("a\0b", 3);
    const std::string expected = "HTTP/1.1 200 OK\r\n"
                                 "Content-Length: 3\r\n"
                                 "Connection: keep-alive\r\n"
                                 "Server: cpp-http-server/0.2\r\n\r\n";
    CHECK(serialize(response, true) == expected + response.body);
    response.omitBody = true;
    CHECK(serialize(response, true) == expected);
}

TEST_CASE("An empty response still has complete framing")
{
    CHECK(serialize(Response{}, false) == "HTTP/1.1 200 OK\r\n"
                                          "Content-Length: 0\r\n"
                                          "Connection: close\r\n"
                                          "Server: cpp-http-server/0.2\r\n\r\n");
}

TEST_CASE("Serializer owns framing and server headers regardless of case")
{
    Response response;
    response.body = "ok";
    response.headers = {{"content-length", "999"},
                        {"CONNECTION", "keep-alive"},
                        {"sErVeR", "wrong"},
                        {"X-Test", "preserved"}};
    CHECK(serialize(response, false) == "HTTP/1.1 200 OK\r\n"
                                        "X-Test: preserved\r\n"
                                        "Content-Length: 2\r\n"
                                        "Connection: close\r\n"
                                        "Server: cpp-http-server/0.2\r\n\r\nok");
}

TEST_CASE("Reason phrases cover every Phase One status")
{
    const std::vector<std::pair<int, std::string>> cases = {
        {100, "Continue"},
        {200, "OK"},
        {204, "No Content"},
        {206, "Partial Content"},
        {304, "Not Modified"},
        {400, "Bad Request"},
        {404, "Not Found"},
        {405, "Method Not Allowed"},
        {408, "Request Timeout"},
        {411, "Length Required"},
        {413, "Content Too Large"},
        {414, "URI Too Long"},
        {416, "Range Not Satisfiable"},
        {417, "Expectation Failed"},
        {431, "Request Header Fields Too Large"},
        {500, "Internal Server Error"},
        {501, "Not Implemented"},
        {505, "HTTP Version Not Supported"}};
    for (const auto &entry : cases)
    {
        INFO(entry.first);
        CHECK(reasonPhrase(entry.first) == entry.second);
    }
}

TEST_CASE("Error responses use the configured document root")
{
    ServerConfig config;
    config.docRoot = TEST_DOC_ROOT;
    const auto response = errorResponse(404, config);
    CHECK(response.status == 404);
    CHECK(response.body == "<h1>Fixture: 404 Not Found</h1>\n");
}

TEST_CASE("Missing error pages get a fallback matching the error status")
{
    ServerConfig config;
    config.docRoot = std::string(TEST_DOC_ROOT) + "/missing-root";
    CHECK(errorResponse(404, config).body == "<h1>404 Not Found</h1>");
    CHECK(errorResponse(405, config).body == "<h1>405 Method Not Allowed</h1>");
    CHECK(errorResponse(500, config).body == "<h1>500 Internal Server Error</h1>");
}
