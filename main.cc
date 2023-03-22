#include <iostream>
#include "tcp_server.hpp"

int main(int argc, char* argv[]){
    if (argc != 2) {
        std::cout << "./prorected port\n";
        exit(4);
    }

    int port = std::atoi(argv[1]);
    TcpServer* server = TcpServer::GetInstance(port);

    std::cout << "我他妈来了\n";

    return 0;
}