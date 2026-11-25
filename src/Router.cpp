#include "Router.h"
#include "FileSystem.h"
#include "Server.h"

Response route(const Request &request, const ServerConfig &config)
{
    if (request.method != "GET")
    {
        auto response = errorResponse(405, config);
        response.headers.emplace_back("Allow", "GET");
        return response;
    }
    if (request.path == "/health")
        return {200, {{"Content-Type", "text/plain"}}, "ok\n"};

    if (request.path.empty() || request.path.front() != '/')
        return errorResponse(404, config);
    const auto fileName = request.path == "/" ? "index.html" : request.path.substr(1);
    auto file = readFile(config.docRoot, fileName);
    if (!file.success)
        return errorResponse(404, config);
    return {200, {{"Content-Type", getContentType(fileName)}}, std::move(file.content)};
}
