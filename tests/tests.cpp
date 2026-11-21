// Minimal tests with no framework: run ./build/http_tests (or ctest).
#include "FileSystem.h"
#include "HttpParser.h"
#include "HttpResponse.h"
#include "Utils.h"

#include <iostream>

static int failures = 0;

#define CHECK(condition)                                                 \
    do                                                                   \
    {                                                                    \
        if (!(condition))                                                \
        {                                                                \
            std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n"; \
            ++failures;                                                  \
        }                                                                \
    } while (0)

static void testParser()
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

    CHECK(!parseRequest("GET /\r\n\r\n").isValid);                       // no version
    CHECK(!parseRequest("GET / FTP/1.0\r\n\r\n").isValid);               // wrong protocol
    CHECK(!parseRequest("GET index.html HTTP/1.1\r\n\r\n").isValid);     // not a /path
    CHECK(!parseRequest("GET /%zz HTTP/1.1\r\n\r\n").isValid);           // bad escape
    CHECK(!parseRequest("GET / HTTP/1.1\r\nNoColon\r\n\r\n").isValid);   // bad header
}

static void testUrlDecode()
{
    std::string out;
    CHECK(urlDecode("a%2Fb", out, false) && out == "a/b");
    CHECK(urlDecode("a+b", out, false) && out == "a+b");
    CHECK(urlDecode("a+b", out, true) && out == "a b");
    CHECK(!urlDecode("%4", out, false));
}

static void testFileSystem()
{
    CHECK(readFile("index.html").success);
    CHECK(!readFile("does-not-exist.html").success);
    CHECK(!readFile("../README.md").success);       // escapes public/
    CHECK(!readFile("../../../../etc/passwd").success);
    CHECK(!readFile("/etc/passwd").success);        // absolute path
    CHECK(!readFile("").success);                   // the directory itself
    CHECK(getContentType("x.html") == "text/html");
    CHECK(getContentType("noext") == "application/octet-stream");
}

static void testResponse()
{
    std::string r = buildResponse("HTTP/1.1 405 Method Not Allowed", "405.html", "hi", "Allow: GET\r\n");
    CHECK(r.rfind("HTTP/1.1 405 Method Not Allowed\r\n", 0) == 0);
    CHECK(r.find("Content-Length: 2\r\n") != std::string::npos);
    CHECK(r.find("Connection: close\r\n") != std::string::npos);
    CHECK(r.find("Allow: GET\r\n") != std::string::npos);
    CHECK(r.size() >= 6 && r.substr(r.size() - 6) == "\r\n\r\nhi");
}

int main()
{
    testParser();
    testUrlDecode();
    testFileSystem();
    testResponse();
    if (failures == 0)
        std::cout << "All tests passed\n";
    return failures == 0 ? 0 : 1;
}
