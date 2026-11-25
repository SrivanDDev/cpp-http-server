#pragma once

#include <atomic>
#include <mutex>
#include <set>
#include <string>

struct ServerConfig
{
    std::string host = "127.0.0.1";
    int port = 8080; // 0 lets the OS choose a free port.
    std::string docRoot = "public";
};

// Start once, then run on one thread. stop() may be called from another thread.
// The caller must join the run() thread before destroying the Server.
class Server
{
  public:
    explicit Server(ServerConfig config);
    ~Server();
    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    bool start();
    int port() const;
    void run();
    void stop();

  private:
    ServerConfig config_;
    int listenFd_ = -1;
    int boundPort_ = 0;
    std::atomic<bool> stopping_{false};
    std::atomic<bool> runStarted_{false};
    std::mutex mutex_;
    std::set<int> clients_;
};
