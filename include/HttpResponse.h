#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct ServerConfig;

struct Response
{
    int status = 200;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    bool omitBody = false;
};

std::string_view reasonPhrase(int status);
// Framing headers are generated here. omitBody preserves the body length for HEAD.
std::string serialize(const Response &response, bool keepAlive);
Response errorResponse(int status, const ServerConfig &config);
