#pragma once
#include<string>
#include<unordered_map>

struct Request{
    std::string method;
    std::string path;
    std::string version;

    std::unordered_map <std::string, std::string> headers;
    std::unordered_map <std::string,std::string> queryParameters;

    std::string body;
    
    bool isValid = false;
};