#include "HttpParser.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("HTTP requests are parsed and decoded")
{
    Request r = parseRequest("GET /a%20b.html?q=hello+world&flag&x=%41 HTTP/1.1\r\n"
                             "Host: localhost\r\n"
                             "Accept: text/html\r\n"
                             "accept: text/plain\r\n"
                             "\r\n");
    CHECK(r.isValid);
    CHECK(r.method == "GET");
    CHECK(r.path == "/a b.html");
    CHECK(r.version == "HTTP/1.1");
    CHECK(r.headers["host"] == "localhost");
    CHECK(r.headers["accept"] == "text/html, text/plain"); // duplicates combined
    CHECK(r.queryParameters["q"] == "hello world");
    CHECK(r.queryParameters.count("flag") == 1);
    CHECK(r.queryParameters["x"] == "A");

    CHECK(!parseRequest("GET /\r\n\r\n").isValid);                     // no version
    CHECK(!parseRequest("GET / FTP/1.0\r\n\r\n").isValid);             // wrong protocol
    CHECK(!parseRequest("GET index.html HTTP/1.1\r\n\r\n").isValid);   // not a /path
    CHECK(!parseRequest("GET /%zz HTTP/1.1\r\n\r\n").isValid);         // bad escape
    CHECK(!parseRequest("GET / HTTP/1.1\r\nNoColon\r\n\r\n").isValid); // bad header
}
