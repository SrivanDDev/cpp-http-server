#include <iostream>
#include <sys/socket.h>  // Core socket functions and structures
#include <unistd.h> //for close function
#include <netinet/in.h> // for sockaddr_in
#include <arpa/inet.h> //for inet_addr,inet_pton etc..
#include<cstring> //for strlen


int main(){

    // create a socket
    int serverSocket = socket(AF_INET,SOCK_STREAM,0);
    if (serverSocket == -1){
        std::cerr <<"[-] Socket creation failed\n";
        return 1;
    }

    std::cout<<"[+] Socket created successfully\n";

    // Configure server address
    
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port=htons(8080);
    int result = inet_pton(AF_INET,"127.0.0.1",&serverAddress.sin_addr);
    if (result!=1){
        std::cerr<<"[-] Invalid IP address\n";
        close(serverSocket);
        return 1;
    }

    // Binding to a port
    
    if (bind(serverSocket,reinterpret_cast<sockaddr*>(&serverAddress),sizeof(serverAddress)) == -1){
        std::cerr<<"[-] Bind failed\n";
        close(serverSocket);
        return 1;
    }

    std::cout<<"[+] Successfully Bound to port 8080\n";

    // start listening for incoming connections

    result = listen(serverSocket , SOMAXCONN);

    if (result == -1)
    {
        std::cerr << "[-] Listen failed\n";
        close(serverSocket);
        return 1;
    }

    std::cout << "[+] Server is listening on 127.0.0.1:8080\n";

    //creating a buufer so that kernel can store data while we accept the client 

    sockaddr_in clientAddress{};
    socklen_t clientAddressLength = sizeof(clientAddress);

    //accepting the client

    std::cout << "[+] Waiting for a client...\n";
    int clientSocket = accept(serverSocket,reinterpret_cast<sockaddr*>(&clientAddress),&clientAddressLength);

    if (clientSocket == -1)
    {
        std::cerr<<"[-] Failed in accepting client\n";
        close(serverSocket);
        return 1;
    }

    std::cout<<"[+] Successfully accepted the client\n";

    //buffer for incoming HTTP request

    char buffer[4096]{};
    ssize_t bytesReceived = recv(clientSocket,buffer,sizeof(buffer),0);
    if(bytesReceived == -1){
        std::cerr<<"[-] Failed to recieve \n";
        close(serverSocket);
        close(clientSocket);
        return 1;
    }
    
    std::cout<<"[+] Received "<<bytesReceived<<" bytes\n";

    std::cout<<"====HTTP REQUEST====\n";
    std::cout.write(buffer,bytesReceived);
    std::cout<<"\n";

    // sending a response to the kernel
    const char* response = "HTTP/1.1 200 OK\r\n""Content-Type: text/plain\r\n""Content-Length: 11\r\n""\r\n""Hello World";

    ssize_t bytesSent = send(clientSocket,response,strlen(response),0);

    if (bytesSent == -1)
    {
        std::cerr<<"[-] Failed in sending response\n";
        close(serverSocket);
        close(clientSocket);
        return 1;
    }
    
    std::cout<<"[+] Sent "<<bytesSent<<" bytes\n";

    close(clientSocket);    
    close(serverSocket);
    return 0;
}