#include "ClientHandler.h"
#include "HttpParser.h"
#include "HttpResponse.h"
#include "FileSystem.h"

#include <cctype>
#include <cerrno>
#include <iostream>
#include <string>
#include <sys/socket.h> // Core socket functions and structures
#include <sys/time.h>   // timeval for socket timeouts
#include <unistd.h>     //for close function

// Limits so one client can't make us buffer unlimited data.
static const size_t MAX_HEADER_BYTES = 8 * 1024;
static const size_t MAX_BODY_BYTES = 1024 * 1024;
static const int SOCKET_TIMEOUT_SECONDS = 10;

// send() may write only part of the buffer, so keep going until it's all out.
static bool sendAll(int clientSocket, const std::string &data)
{
    size_t totalSent = 0;
    while (totalSent < data.size())
    {
        // MSG_NOSIGNAL: if the client hung up, return an error instead of
        // killing the whole server with SIGPIPE.
        ssize_t sent = send(clientSocket, data.data() + totalSent, data.size() - totalSent, MSG_NOSIGNAL);
        if (sent == -1)
        {
            if (errno == EINTR)
                continue;
            return false;
        }
        totalSent += sent;
    }
    return true;
}

// Reads once from the socket and appends to `buffer`.
// Returns bytes read, 0 if the client closed, -1 on error or timeout.
static ssize_t receiveMore(int clientSocket, std::string &buffer)
{
    char chunk[4096];
    ssize_t bytesReceived;
    do
    {
        bytesReceived = recv(clientSocket, chunk, sizeof(chunk), 0);
    } while (bytesReceived == -1 && errno == EINTR);

    if (bytesReceived > 0)
        buffer.append(chunk, bytesReceived);
    return bytesReceived;
}

static void sendErrorAndClose(int clientSocket, const std::string &statusLine, const std::string &message)
{
    std::string body = "<h1>" + message + "</h1>";
    sendAll(clientSocket, buildResponse(statusLine, "error.html", body));
    close(clientSocket);
}

void handleClient(int clientSocket)
{
    // Without a timeout, a client that connects and sends nothing would hold
    // this thread forever.
    timeval timeout{SOCKET_TIMEOUT_SECONDS, 0};
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    // 1. Keep reading until we have the full header block (ends with a blank line).
    std::string rawRequest;
    size_t headerEnd;
    while ((headerEnd = rawRequest.find("\r\n\r\n")) == std::string::npos)
    {
        if (rawRequest.size() > MAX_HEADER_BYTES)
        {
            sendErrorAndClose(clientSocket, "HTTP/1.1 431 Request Header Fields Too Large", "431 Request Header Fields Too Large");
            return;
        }
        ssize_t bytesReceived = receiveMore(clientSocket, rawRequest);
        if (bytesReceived == 0)
        {
            std::cout << "[+] Client disconnected\n";
            close(clientSocket);
            return;
        }
        if (bytesReceived == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                sendErrorAndClose(clientSocket, "HTTP/1.1 408 Request Timeout", "408 Request Timeout");
            else
            {
                std::cerr << "[-] Failed to receive\n";
                close(clientSocket);
            }
            return;
        }
    }

    // 2. Parse the request line and headers.
    Request request = parseRequest(rawRequest.substr(0, headerEnd + 4));
    if (!request.isValid)
    {
        sendErrorAndClose(clientSocket, "HTTP/1.1 400 Bad Request", "400 Bad Request");
        return;
    }

    // 3. Read the body, if the client said it is sending one.
    size_t contentLength = 0;
    auto lengthHeader = request.headers.find("content-length");
    if (lengthHeader != request.headers.end())
    {
        const std::string &value = lengthHeader->second;
        // Digits only: this also rejects duplicated headers like "5, 10".
        bool allDigits = !value.empty() && value.size() <= 18;
        for (unsigned char c : value)
            allDigits = allDigits && std::isdigit(c);
        if (!allDigits)
        {
            sendErrorAndClose(clientSocket, "HTTP/1.1 400 Bad Request", "400 Bad Request");
            return;
        }
        contentLength = std::stoull(value);
        if (contentLength > MAX_BODY_BYTES)
        {
            sendErrorAndClose(clientSocket, "HTTP/1.1 413 Content Too Large", "413 Content Too Large");
            return;
        }
    }
    size_t bodyStart = headerEnd + 4;
    while (rawRequest.size() - bodyStart < contentLength)
    {
        if (receiveMore(clientSocket, rawRequest) <= 0)
        {
            std::cerr << "[-] Client stopped before sending the full body\n";
            close(clientSocket);
            return;
        }
    }
    request.body = rawRequest.substr(bodyStart, contentLength);

    // routing
    FileResult body;
    std::string statusLine = "HTTP/1.1 200 OK";
    std::string fileName;
    std::string extraHeaders;

    if (request.method == "GET" && request.path == "/health")
    {
        fileName = "health.txt";
        body.success = true;
        body.content = "ok\n";
    }
    else if (request.method != "GET")
    {
        statusLine = "HTTP/1.1 405 Method Not Allowed";
        // HTTP requires a 405 response to say which methods are allowed.
        extraHeaders = "Allow: GET\r\n";
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

    std::cout << "[+] " << request.method << ' ' << request.path << " -> " << statusLine.substr(9) << '\n';

    // sending a response to the kernel
    std::string response = buildResponse(statusLine, fileName, body.content, extraHeaders);
    if (!sendAll(clientSocket, response))
    {
        std::cerr << "[-] Failed in sending response\n";
    }
    close(clientSocket);
}
