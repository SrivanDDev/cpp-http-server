#include "ClientHandler.h"

#include <arpa/inet.h> //for inet_pton
#include <cerrno>
#include <cstring> //for strerror
#include <iostream>
#include <netinet/in.h> // for sockaddr_in
#include <string>       //for String functions
#include <sys/socket.h> // Core socket functions and structures
#include <system_error>
#include <thread>
#include <unistd.h>     //for close function

int main(int argc, char* argv[])
{
    int port = 8080;
    if (argc > 1)
    {
        try
        {
            port = std::stoi(argv[1]);
        }
        catch (const std::exception&)
        {
            std::cerr << "[-] Port must be a number\n";
            return 1;
        }
    }

    if (port < 1 || port > 65535)
    {
        std::cerr << "[-] Port must be between 1 and 65535\n";
        return 1;
    }

    // create a socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        std::cerr << "[-] Socket creation failed\n";
        return 1;
    }

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
        -1)
    {
        std::cerr << "[-] Failed to set socket options\n";
        close(serverSocket);
        return 1;
    }

    std::cout << "[+] Socket created successfully\n";

    // Configure server address

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    int result = inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);
    if (result != 1)
    {
        std::cerr << "[-] Invalid IP address\n";
        close(serverSocket);
        return 1;
    }

    // Binding to a port

    if (bind(serverSocket, reinterpret_cast<sockaddr *>(&serverAddress),
             sizeof(serverAddress)) == -1)
    {
        std::cerr << "[-] Bind failed\n"
                  << strerror(errno);
        close(serverSocket);
        return 1;
    }

    std::cout << "[+] Successfully Bound to port " << port << "\n";

    // start listening for incoming connections

    result = listen(serverSocket, SOMAXCONN);

    if (result == -1)
    {
        std::cerr << "[-] Listen failed\n";
        close(serverSocket);
        return 1;
    }

    std::cout << "[+] Server is listening on 127.0.0.1:" << port << "\n";
    while (true)
    {
        // creating a buufer so that kernel can store data while we accept the
        // client

        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);

        // accepting the client

        std::cout << "[+] Waiting for a client...\n";

        int clientSocket =
            accept(serverSocket, reinterpret_cast<sockaddr *>(&clientAddress),
                   &clientAddressLength);

        if (clientSocket == -1)
        {
            std::cerr << "[-] Failed in accepting client\n";
            continue;
        }

        std::cout << "[+] Successfully accepted the client\n";

        // Handle each client on its own thread so one slow client doesn't
        // block everyone else. detach() lets the thread clean itself up;
        // handleClient always closes the socket when it's done.
        try
        {
            std::thread(handleClient, clientSocket).detach();
        }
        catch (const std::system_error &)
        {
            std::cerr << "[-] Could not start a thread for the client\n";
            close(clientSocket);
        }
    }
    close(serverSocket);
    return 0;
}
