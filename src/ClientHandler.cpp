#include "ClientHandler.h"
#include "HttpParser.h"
#include "HttpResponse.h"
#include "FileSystem.h"

#include <iostream>
#include <sys/socket.h> // Core socket functions and structures
#include <unistd.h>     //for close function

void handleClient(int clientSocket)
{
    // buffer for incoming HTTP request

    char buffer[4096]{};
    ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesReceived == 0)
    {
        std::cout << "[+] Client disconnected\n";
        close(clientSocket);
        return;
    }
    if (bytesReceived == -1)
    {
        std::cerr << "[-] Failed to receive \n";

        close(clientSocket);
        return;
    }

    std::cout << "[+] Received " << bytesReceived << " bytes\n";

    // parsing the request

    std::string rawRequest(buffer, bytesReceived); // converting buffer from char to string
    std::cout << rawRequest + "\n";
    Request request = parseRequest(rawRequest);
    if (!request.isValid)
    {
        close(clientSocket);
        return;
    }

    // temporary stuff
    std::cout << "\n===== Parsed Request =====\n";
    std::cout << "Method : " << request.method << '\n';
    std::cout << "Path   : " << request.path << '\n';
    std::cout << "Version: " << request.version << '\n';

    // routing
    FileResult body;
    std::string statusLine = "HTTP/1.1 200 OK";
    std::string fileName;

    if (request.method != "GET")
    {
        statusLine = "HTTP/1.1 405 Method Not Allowed";
        fileName = "405.html";
        body = readFile(fileName);

        if (!body.success)
        {
            body.success = true;
            body.content = "<h1>405 Method Not Allowed</h1>";
        }
    }
    else
    {
        if (request.path == "/")
        {
            fileName = "index.html";
        }
        else
        {
            fileName = request.path.substr(1);
        }

        body = readFile(fileName);
        if (!body.success)
        {
            statusLine = "HTTP/1.1 404 Not Found";
            fileName = "404.html";
            body = readFile(fileName);
            if (!body.success)
            {
                body.success = true;
                body.content = "<h1>404 Not Found</h1>";
            }
        }
    }

    // sending a response to the kernel
    std::string response = buildResponse(statusLine, fileName, body.content);

    ssize_t bytesSent = send(clientSocket, response.c_str(), response.size(), 0);

    if (bytesSent == -1)
    {
        std::cerr << "[-] Failed in sending response\n";
        close(clientSocket);
        return;
    }

    std::cout << "[+] Sent " << bytesSent << " bytes\n";

    close(clientSocket);
}