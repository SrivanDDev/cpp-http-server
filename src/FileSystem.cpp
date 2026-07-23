#include "FileSystem.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>

FileResult readFile(const std::string& path){
    FileResult result{false,""};
    std::cout << "Trying to open: public/" << path << '\n';
    std::ifstream file("public/"+path);
    if (!file)
    {
        std::cerr << "[-] Failed to open file: " << path << '\n';
        return result;
    }
    result.success=true;
    std::stringstream ss;
    ss<<file.rdbuf();
    result.content = ss.str();
    return result;
}

std::string getContentType(const std::string& fileName){
    size_t dot = fileName.find_last_of(".");
    if (dot == std::string::npos)
    {
        return "application/octet-stream";
    }
    std::string type = fileName.substr(dot+1);
    static const std::unordered_map<std::string, std::string> mimeTypes ={
        {"html", "text/html"},
        {"css",  "text/css"},
        {"js",   "application/javascript"},
        {"png",  "image/png"},
        {"jpg",  "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif",  "image/gif"},
        {"svg",  "image/svg+xml"},
        {"ico",  "image/x-icon"},
        {"txt",  "text/plain"},
        {"json", "application/json"}
    };
    auto it = mimeTypes.find(type);
    if (it != mimeTypes.end())
    {
        return it->second;
    }
    return "application/octet-stream";
}
