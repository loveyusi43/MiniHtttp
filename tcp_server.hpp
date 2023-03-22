#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <mutex>


// TCP底层全链接队列的长度
static constexpr int BAKLOG = 10;

class TcpServer{
public:
    static TcpServer* GetInstance(int port) {
        static std::mutex mtx{};
        if (server_ == nullptr){
            mtx.lock();
            if (nullptr == server_){
                server_ = new TcpServer(port);
            }
            mtx.unlock();
        }
        return server_;
    }

protected:
    TcpServer(int port) : port_(port), listen_sock_(-1) {
        Socket();
        Bind();
        Listen();
    }

    TcpServer(const TcpServer&) = delete;

    TcpServer operator=(const TcpServer&) = delete;

    void Socket() {
        listen_sock_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_sock_ < 0){
            std::cout << "socket error\n";
            exit(1);
        }
        // socket地址重用
        int opt = 1;
        setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    void Bind() {
        struct sockaddr_in local;
        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = INADDR_ANY;
        local.sin_port = htons(port_);

        if (bind(listen_sock_, (struct sockaddr*)&local, sizeof(local)) < 0) {
            std::cout << "bind error\n";
            exit(2);
        }
    }

    void Listen() {
        if (listen(listen_sock_, BAKLOG) < 0) {
            std::cout << "listen error\n";
            exit(3);
        }
    }

    int GetSock() {
        return listen_sock_;
    }

    void Cenecte();

protected:
    int port_ = 8080;
    int listen_sock_ = -1;
    static TcpServer* server_;
};

TcpServer* TcpServer::server_ = nullptr;

#endif  // TCP_SERVER_HPP