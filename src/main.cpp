#include "FileSystem.h"
#include "Request.h"
#include "HttpParser.h"
#include "HttpResponse.h"
#include "ClientHandler.h"


#include <arpa/inet.h> //for inet_addr,inet_pton etc..
#include <cerrno>
#include <cstring> //for strlen
#include <iostream>
#include <netinet/in.h> // for sockaddr_in

#include <string>       //for String functions
#include <sys/socket.h> // Core socket functions and structures
#include <unistd.h>     //for close function



int main()
{

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
    serverAddress.sin_port = htons(8080);
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

    std::cout << "[+] Successfully Bound to port 8080\n";

    // start listening for incoming connections

    result = listen(serverSocket, SOMAXCONN);

    if (result == -1)
    {
        std::cerr << "[-] Listen failed\n";
        close(serverSocket);
        return 1;
    }

    std::cout << "[+] Server is listening on 127.0.0.1:8080\n";
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

        // handles the client
        handleClient(clientSocket);
    }
    close(serverSocket);
    return 0;
}