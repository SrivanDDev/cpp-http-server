#include "ClientHandler.h"
#include "HttpParser.h"
#include "HttpResponse.h"
#include "Router.h"

#include <cctype>
#include <cerrno>
#include <iostream>
#include <string>
#include <sys/socket.h> // Core socket functions and structures
#include <sys/time.h>   // timeval for socket timeouts

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
        ssize_t sent =
            send(clientSocket, data.data() + totalSent, data.size() - totalSent, MSG_NOSIGNAL);
        if (sent == -1)
        {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (sent == 0)
            return false;
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

static void sendError(int clientSocket, int status, const ServerConfig &config)
{
    sendAll(clientSocket, serialize(errorResponse(status, config), false));
}

void handleClient(int clientSocket, const ServerConfig &config)
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
            sendError(clientSocket, 431, config);
            return;
        }
        ssize_t bytesReceived = receiveMore(clientSocket, rawRequest);
        if (bytesReceived == 0)
        {
            std::cout << "[+] Client disconnected\n";
            return;
        }
        if (bytesReceived == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                sendError(clientSocket, 408, config);
            else
            {
                std::cerr << "[-] Failed to receive\n";
            }
            return;
        }
    }

    // The last recv() may have delivered both the overflow and the terminator,
    // so check the final header size too.
    if (headerEnd + 4 > MAX_HEADER_BYTES)
    {
        sendError(clientSocket, 431, config);
        return;
    }

    // 2. Parse the request line and headers.
    Request request = parseRequest(rawRequest.substr(0, headerEnd + 4));
    if (!request.isValid)
    {
        sendError(clientSocket, 400, config);
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
            sendError(clientSocket, 400, config);
            return;
        }
        contentLength = std::stoull(value);
        if (contentLength > MAX_BODY_BYTES)
        {
            sendError(clientSocket, 413, config);
            return;
        }
    }
    size_t bodyStart = headerEnd + 4;
    while (rawRequest.size() - bodyStart < contentLength)
    {
        if (receiveMore(clientSocket, rawRequest) <= 0)
        {
            std::cerr << "[-] Client stopped before sending the full body\n";
            return;
        }
    }
    request.body = rawRequest.substr(bodyStart, contentLength);

    const Response response = route(request, config);
    std::cout << "[+] " << request.method << ' ' << request.path << " -> " << response.status << ' '
              << reasonPhrase(response.status) << '\n';

    // Persistent connections arrive in M4; M2 keeps one response per connection.
    if (!sendAll(clientSocket, serialize(response, false)))
        std::cerr << "[-] Failed in sending response\n";
}
