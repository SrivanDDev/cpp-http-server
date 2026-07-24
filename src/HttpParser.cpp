#include "HttpParser.h"
#include "Request.h"
#include "Utils.h"
#include<iostream>
#include<sstream>

Request parseRequest(const std::string& rawRequest){
    Request request;

    std::string line;
    std::stringstream ss(rawRequest);
    if (!(ss>>request.method>>request.path>>request.version))
    {
        std::cout<<"[-] Invalid HTTP request\n";
        return request;
    }

    while (std::getline(ss,line))
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
        size_t colonPostion = line.find(":");
        if (colonPostion == std::string::npos)
        {
            std::cerr<<"[-] Malformed header";
            return request;
        }
        // splitting header into key value pairs

        std::string value1 = line.substr(0,colonPostion);
        std::string value2 = line.substr(colonPostion+1);
        request.headers.emplace(toLower((value1)),toLower((value2)));
    }
    request.isValid = true;
    return request;
}