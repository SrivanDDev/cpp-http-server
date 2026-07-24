#include "HttpResponse.h"
#include "FileSystem.h"

std::string buildResponse(const std::string &statusLine, const std::string &fileName, const std::string &content)
{
    std::string response = statusLine + "\r\n";
    response += "Content-Type: " + getContentType(fileName) + "\r\n";
    response += "Content-Length: " + std::to_string(content.size()) + "\r\n";
    response += "\r\n";
    response += content;
    return response;
}