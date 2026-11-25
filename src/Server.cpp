#include "Server.h"
#include "ClientHandler.h"

#include <arpa/inet.h>
#include <cerrno>
#include <list>
#include <memory>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>

Server::Server(ServerConfig config) : config_(std::move(config)) {}

Server::~Server()
{
    stop();
    if (listenFd_ != -1)
        close(listenFd_);
}

bool Server::start()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (listenFd_ != -1 || stopping_ || config_.port < 0 || config_.port > 65535)
        return false;

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(config_.port));
    if (inet_pton(AF_INET, config_.host.c_str(), &address.sin_addr) != 1)
        return false;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        return false;
    int reuse = 1;
    socklen_t length = sizeof(address);
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1 ||
        bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == -1 ||
        listen(fd, SOMAXCONN) == -1 ||
        getsockname(fd, reinterpret_cast<sockaddr *>(&address), &length) == -1)
    {
        close(fd);
        return false;
    }
    listenFd_ = fd;
    boundPort_ = ntohs(address.sin_port);
    return true;
}

int Server::port() const
{
    return boundPort_;
}

void Server::stop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
    // Shutdown wakes accept()/recv(). The owning threads close their descriptors.
    if (listenFd_ != -1)
        shutdown(listenFd_, SHUT_RDWR);
    for (int client : clients_)
        shutdown(client, SHUT_RDWR);
}

void Server::run()
{
    if (listenFd_ == -1 || runStarted_.exchange(true))
        return;

    struct Worker
    {
        std::thread thread;
        std::shared_ptr<std::atomic<bool>> done;
    };
    std::list<Worker> workers;
    while (!stopping_)
    {
        int client = accept(listenFd_, nullptr, nullptr);
        if (client == -1)
        {
            if (errno == EINTR)
                continue;
            break;
        }

        // Reap completed threads instead of accumulating them until shutdown.
        for (auto it = workers.begin(); it != workers.end();)
        {
            if (*it->done)
            {
                it->thread.join();
                it = workers.erase(it);
            }
            else
                ++it;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_)
        {
            close(client);
            break;
        }
        try
        {
            clients_.insert(client);
            workers.emplace_back();
            auto &worker = workers.back();
            worker.done = std::make_shared<std::atomic<bool>>(false);
            worker.thread = std::thread(
                [this, client, config = config_, done = worker.done]
                {
                    try
                    {
                        handleClient(client, config);
                    }
                    catch (...)
                    {
                        // Keep an individual connection failure inside its worker.
                    }
                    {
                        std::lock_guard<std::mutex> clientLock(mutex_);
                        clients_.erase(client);
                        close(client);
                    }
                    *done = true;
                });
        }
        catch (...)
        {
            if (!workers.empty() && !workers.back().thread.joinable())
                workers.pop_back();
            clients_.erase(client);
            close(client);
        }
    }

    stop();
    for (auto &worker : workers)
        worker.thread.join();
}
