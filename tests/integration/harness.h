#pragma once

#include "Server.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <cerrno>
#include <map>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

class TestServer
{
  public:
    explicit TestServer(std::string docRoot = TEST_DOC_ROOT)
        : server_({"127.0.0.1", 0, std::move(docRoot)})
    {
        if (!server_.start())
            throw std::runtime_error("Could not start test server");
        thread_ = std::thread([this] { server_.run(); });
    }

    ~TestServer()
    {
        server_.stop();
        if (thread_.joinable())
            thread_.join();
    }

    TestServer(const TestServer &) = delete;
    TestServer &operator=(const TestServer &) = delete;

    int port() const
    {
        return server_.port();
    }
    void stop()
    {
        server_.stop();
    }

  private:
    Server server_;
    std::thread thread_;
};

class TestSocket
{
  public:
    explicit TestSocket(int port)
    {
        fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ == -1)
            throw std::runtime_error("Could not create test socket");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<uint16_t>(port));
        inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
        timeval timeout{2, 0};
        if (setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1 ||
            setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1 ||
            connect(fd_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == -1)
        {
            close(fd_);
            throw std::runtime_error("Could not connect test socket");
        }
    }

    ~TestSocket()
    {
        close(fd_);
    }
    TestSocket(const TestSocket &) = delete;
    TestSocket &operator=(const TestSocket &) = delete;

    void sendBytes(const std::string &bytes)
    {
        size_t sent = 0;
        while (sent < bytes.size())
        {
            ssize_t count = send(fd_, bytes.data() + sent, bytes.size() - sent, MSG_NOSIGNAL);
            if (count == -1 && errno == EINTR)
                continue;
            if (count <= 0)
                throw std::runtime_error("Could not send test request");
            sent += static_cast<size_t>(count);
        }
    }

    std::string receiveUntilClose()
    {
        std::string response;
        char chunk[4096];
        while (true)
        {
            ssize_t count = recv(fd_, chunk, sizeof(chunk), 0);
            if (count > 0)
                response.append(chunk, static_cast<size_t>(count));
            else if (count == 0)
                break;
            else if (errno == EINTR)
                continue;
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else
                throw std::runtime_error("Could not read test response");
        }
        return response;
    }

  private:
    int fd_;
};

inline std::string sendRaw(int port, const std::string &bytes)
{
    TestSocket socket(port);
    socket.sendBytes(bytes);
    return socket.receiveUntilClose();
}

struct ParsedResponse
{
    int status;
    std::map<std::string, std::string> headers;
    std::string body;
};

inline std::vector<ParsedResponse> parseResponses(const std::string &raw)
{
    std::vector<ParsedResponse> responses;
    size_t position = 0;
    while (position < raw.size())
    {
        size_t headEnd = raw.find("\r\n\r\n", position);
        size_t lineEnd = raw.find("\r\n", position);
        if (headEnd == std::string::npos || lineEnd == std::string::npos)
            throw std::runtime_error("Incomplete response head");
        std::string statusLine = raw.substr(position, lineEnd - position);
        if (statusLine.size() < 12 || statusLine.compare(0, 5, "HTTP/") != 0 ||
            statusLine[8] != ' ' ||
            !std::all_of(statusLine.begin() + 9, statusLine.begin() + 12,
                         [](unsigned char c) { return c >= '0' && c <= '9'; }))
            throw std::runtime_error("Invalid response status");
        ParsedResponse response{};
        response.status = std::stoi(statusLine.substr(9, 3));
        position = lineEnd + 2;
        while (position < headEnd)
        {
            lineEnd = raw.find("\r\n", position);
            std::string line = raw.substr(position, lineEnd - position);
            size_t colon = line.find(':');
            if (colon == std::string::npos || colon == 0)
                throw std::runtime_error("Invalid response header");
            std::string name = line.substr(0, colon);
            std::transform(name.begin(), name.end(), name.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            size_t start = line.find_first_not_of(" \t", colon + 1);
            size_t end = line.find_last_not_of(" \t");
            response.headers[name] =
                start == std::string::npos ? "" : line.substr(start, end - start + 1);
            position = lineEnd + 2;
        }
        auto length = response.headers.find("content-length");
        if (length == response.headers.end() || length->second.empty() ||
            !std::all_of(length->second.begin(), length->second.end(),
                         [](unsigned char c) { return c >= '0' && c <= '9'; }))
            throw std::runtime_error("Invalid or missing Content-Length");
        auto bodyLength = std::stoull(length->second);
        position = headEnd + 4;
        if (bodyLength > raw.size() - position)
            throw std::runtime_error("Incomplete response body");
        response.body = raw.substr(position, static_cast<size_t>(bodyLength));
        position += static_cast<size_t>(bodyLength);
        responses.push_back(std::move(response));
    }
    return responses;
}
