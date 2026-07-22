#include <iostream>
#include <sys/socket.h>  // Core socket functions and structures
#include <unistd.h> //for close function
#include <netinet/in.h> // for sockaddr_in
#include <arpa/inet.h> //for inet_addr,inet_pton etc..
#include<cstring> //for strlen
#include<sstream> //for stringStream
#include<string> //for String functions
#include<fstream> //for file I/O
#include<unordered_map> //for unordered_map
#include<cerrno> 
struct FileResult
{
    bool success;
    std::string content;
};

FileResult readFile(const std::string& path){
    FileResult result{false,""};
    std::ifstream file("public/"+path);
    if (!file)
    {
        std::cerr << "[-] Failed to open file: " << path << '\n';
        return result;
    }
    result.success=true;
    std::stringstream ss;
    ss<<file.rdbuf();
    result.content = ss.str();
    return result;
}

std::string getContentType(const std::string& fileName){
    size_t dot = fileName.find_last_of(".");
    if (dot == std::string::npos)
    {
        return "application/octet-stream";
    }
    std::string type = fileName.substr(dot+1);
    static const std::unordered_map<std::string, std::string> mimeTypes ={
        {"html", "text/html"},
        {"css",  "text/css"},
        {"js",   "application/javascript"},
        {"png",  "image/png"},
        {"jpg",  "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif",  "image/gif"},
        {"svg",  "image/svg+xml"},
        {"ico",  "image/x-icon"},
        {"txt",  "text/plain"},
        {"json", "application/json"}
    };
    auto it = mimeTypes.find(type);
    if (it != mimeTypes.end())
    {
        return it->second;
    }
    return "application/octet-stream";
}

int main(){

    // create a socket
    int serverSocket = socket(AF_INET,SOCK_STREAM,0);
    if (serverSocket == -1){
        std::cerr <<"[-] Socket creation failed\n";
        return 1;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
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
        std::cerr<<"[-] Bind failed\n"<<strerror(errno);
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
        close(clientSocket);
        close(serverSocket);
        return 1;
    }
    
    std::cout<<"[+] Received "<<bytesReceived<<" bytes\n";

    // parsing the request 

    std::string request(buffer,bytesReceived); // converting buffer from char to string
    std::cout<<request<<"\n";
    size_t endOfLine = request.find("\r\n"); //finding the end of request line
    if (endOfLine == std::string::npos)
    {
        std::cout<<"[-] Malformed HTTP request\n";

        close(clientSocket);
        close(serverSocket);
        return 1;
    }
    
    std::string requestLine =request.substr(0,endOfLine); //storing the requestline 
    std::stringstream ss(requestLine); //converting into stringstream
    std::string method;
    std::string path;
    std::string version;
    if(!(ss>>method>>path>>version)){   //storing request
        std::cerr<<"[-] Invlaid HTTP request\n";

        close(clientSocket);
        close(serverSocket);
        return 1;
    } 

    // temporary stuff
    std::cout << "\n===== Parsed Request =====\n";
    std::cout << "Method : " << method << '\n';
    std::cout << "Path   : " << path << '\n';
    std::cout << "Version: " << version << '\n';

    // basic routing
    FileResult body; 
    std::string statusLine = "HTTP/1.1 200 OK";
    std::string fileName;

    if (method != "GET"){
        statusLine = "HTTP/1.1 405 Method Not Allowed";
        fileName = "405.html";
        body = readFile(fileName);
        
        if (!body.success)
        {
            body.success = true;
            body.content = "<h1>405 Method Not Allowed</h1>";
        }
    }else{ 
        if (path == "/"){
                fileName = "index.html";
        }
        else{
                fileName = path.substr(1);
        }

        body = readFile(fileName);
        if (!body.success)
        {
            statusLine ="HTTP/1.1 404 Not Found";
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
    std::string response = statusLine+ "\r\n";
    response += "Content-Type: "+getContentType(fileName)+"\r\n";
    response += "Content-Length: " + std::to_string(body.content.size()) + "\r\n";
    response += "\r\n";
    response += body.content;
    ssize_t bytesSent = send(clientSocket,response.c_str(),response.size(),0);

    if (bytesSent == -1)
    {
        std::cerr<<"[-] Failed in sending response\n";
        close(clientSocket);
        close(serverSocket);
        return 1;
    }
    
    std::cout<<"[+] Sent "<<bytesSent<<" bytes\n";

    close(clientSocket);    
    close(serverSocket);
    return 0;
}