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
    // Only accept what we actually speak, and only absolute paths like "/index.html".
    if (request.version != "HTTP/1.0" && request.version != "HTTP/1.1")
    {
        return false;
    }
    if (request.path.empty() || request.path[0] != '/')
    {
        return false;
    }
    // ">>" stops at whitespace and leaves the rest of the line behind; consume it
    // so parseHeaders starts on the first header line.
    std::string rest;
    std::getline(ss, rest);
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
            std::cerr << "[-] Malformed header\n";
            return false;
        }
        // splitting header into key value pairs

        std::string headerName = toLower(trim(line.substr(0, colonPosition)));
        std::string headerValue = trim(line.substr(colonPosition + 1));

        // A repeated header is equivalent to one header with the values joined by ", ".
        auto existing = request.headers.find(headerName);
        if (existing != request.headers.end())
        {
            existing->second += ", " + headerValue;
        }
        else
        {
            request.headers.emplace(headerName, headerValue);
        }
    }
    return true;
}

bool parseQueryParameters(Request &request)
{
    std::string target = request.path; // e.g. "/search?q=hello%20world"
    size_t questionPosition = target.find('?');

    // Decode the path so "/about%2Ehtml" means "/about.html".
    // The traversal check happens later on this decoded path (see FileSystem.cpp).
    if (!urlDecode(target.substr(0, questionPosition), request.path, false))
    {
        return false;
    }
    if (questionPosition == std::string::npos)
    {
        return true;
    }

    auto parameters = split(target.substr(questionPosition + 1), '&');
    for (const auto &parameter : parameters)
    {
        if (parameter.empty())
        {
            continue;
        }
        // "key=value" -> key, value.  "flag" (no '=') -> flag, "".
        size_t equalPosition = parameter.find('=');
        std::string key, value;
        if (!urlDecode(parameter.substr(0, equalPosition), key, true))
        {
            return false;
        }
        if (equalPosition != std::string::npos &&
            !urlDecode(parameter.substr(equalPosition + 1), value, true))
        {
            return false;
        }
        request.queryParameters.emplace(key, value);
    }
    return true;
}

Request parseRequest(const std::string &rawRequest)
{
    Request request;

    std::stringstream ss(rawRequest);

    if (!parseRequestLine(request, ss))
    {
        return request;
    }
    if (!parseQueryParameters(request))
    {
        return request;
    }
    if (!parseHeaders(request, ss))
    {
        return request;
    }
    request.isValid = true;
    return request;
}
