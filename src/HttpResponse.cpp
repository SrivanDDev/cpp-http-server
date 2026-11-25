#include "HttpResponse.h"
#include "FileSystem.h"
#include "Server.h"
#include "Utils.h"

std::string_view reasonPhrase(int status)
{
    switch (status)
    {
    case 100:
        return "Continue";
    case 200:
        return "OK";
    case 204:
        return "No Content";
    case 206:
        return "Partial Content";
    case 304:
        return "Not Modified";
    case 400:
        return "Bad Request";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 408:
        return "Request Timeout";
    case 411:
        return "Length Required";
    case 413:
        return "Content Too Large";
    case 414:
        return "URI Too Long";
    case 416:
        return "Range Not Satisfiable";
    case 417:
        return "Expectation Failed";
    case 431:
        return "Request Header Fields Too Large";
    case 500:
        return "Internal Server Error";
    case 501:
        return "Not Implemented";
    case 505:
        return "HTTP Version Not Supported";
    default:
        return "Unknown Status";
    }
}

std::string serialize(const Response &response, bool keepAlive)
{
    std::string bytes = "HTTP/1.1 " + std::to_string(response.status) + " " +
                        std::string(reasonPhrase(response.status)) + "\r\n";
    for (const auto &header : response.headers)
    {
        // Keep a single authoritative value for framing and server identity.
        const auto name = toLower(header.first);
        if (name == "content-length" || name == "connection" || name == "server")
            continue;
        bytes += header.first + ": " + header.second + "\r\n";
    }
    bytes += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
    bytes += keepAlive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
    bytes += "Server: cpp-http-server/0.2\r\n\r\n";
    if (!response.omitBody)
        bytes += response.body;
    return bytes;
}

Response errorResponse(int status, const ServerConfig &config)
{
    Response response;
    response.status = status;
    response.headers.emplace_back("Content-Type", "text/html");
    auto file = readFile(config.docRoot, std::to_string(status) + ".html");
    if (file.success)
        response.body = std::move(file.content);
    else
        response.body =
            "<h1>" + std::to_string(status) + " " + std::string(reasonPhrase(status)) + "</h1>";
    return response;
}
