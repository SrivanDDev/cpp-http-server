#include "HttpResponse.h"
#include "FileSystem.h"

std::string buildResponse(const std::string &statusLine, const std::string &fileName, const std::string &content, const std::string &extraHeaders)
{
    std::string response = statusLine + "\r\n";
    response += "Content-Type: " + getContentType(fileName) + "\r\n";
    response += "Content-Length: " + std::to_string(content.size()) + "\r\n";
    // We close the socket after every response, so tell the client up front.
    response += "Connection: close\r\n";
    response += extraHeaders;
    response += "\r\n";
    response += content;
    return response;
}
