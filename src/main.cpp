#include "Server.h"

#include <csignal>
#include <iostream>
#include <pthread.h>
#include <stdexcept>
#include <string>
#include <thread>

int main(int argc, char *argv[])
{
    ServerConfig config;
    if (argc > 3)
    {
        std::cerr << "Usage: " << argv[0] << " [port] [docRoot]\n";
        return 1;
    }
    if (argc > 1)
    {
        try
        {
            std::string argument = argv[1];
            size_t consumed = 0;
            config.port = std::stoi(argument, &consumed);
            if (consumed != argument.size() || config.port < 1 || config.port > 65535)
                throw std::invalid_argument("port");
        }
        catch (const std::exception &)
        {
            std::cerr << "[-] Port must be a number between 1 and 65535\n";
            return 1;
        }
    }
    if (argc > 2)
        config.docRoot = argv[2];

    // All threads inherit this mask. sigwait calls stop() on a normal thread,
    // avoiding mutexes and other unsafe operations inside a signal handler.
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    if (pthread_sigmask(SIG_BLOCK, &signals, nullptr) != 0)
    {
        std::cerr << "[-] Could not configure signal handling\n";
        return 1;
    }

    Server server(config);
    if (!server.start())
    {
        std::cerr << "[-] Could not start server on " << config.host << ':' << config.port << '\n';
        return 1;
    }

    std::thread signalThread;
    try
    {
        signalThread = std::thread(
            [&]
            {
                int received = 0;
                if (sigwait(&signals, &received) == 0)
                    server.stop();
            });
        std::cout << "[+] Server is listening on " << config.host << ':' << server.port()
                  << std::endl;
        server.run();
    }
    catch (const std::exception &error)
    {
        std::cerr << "[-] Server failed: " << error.what() << '\n';
        server.stop();
        if (signalThread.joinable())
        {
            pthread_kill(signalThread.native_handle(), SIGTERM);
            signalThread.join();
        }
        return 1;
    }
    // Release sigwait if the accept loop ended because of a socket error.
    pthread_kill(signalThread.native_handle(), SIGTERM);
    signalThread.join();
    return 0;
}
