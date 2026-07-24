#include "HttpParser.h"
#include "Request.h"
#include "Utils.h"
#include <iostream>
#include <sstream>

bool parseRequestLine(Request &request, std::stringstream &ss)
{
    if (!(ss >> request.method >> request.path >> request.version))
    {
        return false;
    }
    return true;
}

bool parseHeaders(Request &request, std::stringstream &ss)
{
    std::string line;
    while (std::getline(ss, line))
    {
        // removing "\r" at the end of ever line and checking the empty lines for the end.
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.empty())
        {
            break;
        }
        // caluclating the : position
        size_t colonPosition = line.find(':');
        if (colonPosition == std::string::npos)
        {
            std::cerr << "[-] Malformed header";
            return false;
        }
        // splitting header into key value pairs

        std::string headerName = line.substr(0, colonPosition);
        std::string headerValue = line.substr(colonPosition + 1);
        request.headers.emplace(toLower(trim(headerName)), trim(headerValue));
    }
    return true;
}

void parseQueryParameters(Request &request)
{
    size_t questionPosition = request.path.find('?');
    if (questionPosition == std::string::npos)
    {
        return;
    }
    std::string queryString = request.path.substr(questionPosition + 1);
    if (queryString.empty())
    {
        request.path = request.path.substr(0, questionPosition);
        return;
    }
    auto parameters = split(queryString, '&');
    for (const auto &parameter : parameters)
    {
        auto keyValue = split(parameter, '=');
        if (keyValue.size() == 2)
        {
            request.queryParameters.emplace(keyValue[0], keyValue[1]);
        }
    }

    request.path = request.path.substr(0, questionPosition);
}

Request parseRequest(const std::string &rawRequest)
{
    Request request;

    std::stringstream ss(rawRequest);

    if (!parseRequestLine(request, ss))
    {
        return request;
    }
    parseQueryParameters(request);
    std::cout << "[+] Parsed Request Line\n";
    if (!parseHeaders(request, ss))
    {
        return request;
    }
    std::cout << "[+] Parsed headers\n";
    request.isValid = true;
    return request;
}