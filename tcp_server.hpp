#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <mutex>

#include "log.hpp"

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

    ~TcpServer() {
        if (listen_sock_ > 0) {
            close(listen_sock_);
        }
    }

protected:
    TcpServer(int port) : port_(port), listen_sock_(-1) {
        Socket();
        Bind();
        Listen();
        LOG(INFO, "tcp server init success!");
    }

    TcpServer(const TcpServer&) = delete;

    TcpServer operator=(const TcpServer&) = delete;

    void Socket() {
        listen_sock_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_sock_ < 0){
            //std::cout << "socket error\n";
            LOG(FATAL, "socket error");
            exit(1);
        }
        // socket地址重用
        int opt = 1;
        setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        LOG(INFO, "create socket success!");
    }

    void Bind() {
        struct sockaddr_in local;
        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = INADDR_ANY;
        local.sin_port = htons(port_);

        if (bind(listen_sock_, /*(struct sockaddr*)&local*/ reinterpret_cast<struct sockaddr*>(&local), sizeof(local)) < 0) {
            // std::cout << "bind error\n";
            LOG(FATAL, "bind error");
            exit(2);
        }
        LOG(INFO, "bind socket success!");
    }

    void Listen() {
        if (listen(listen_sock_, BAKLOG) < 0) {
            //std::cout << "listen error\n";
            LOG(FATAL, "listen socket error!");
            exit(3);
        }
        LOG(INFO, "listen socket success!");
    }

public:
    int GetSock() {
        return listen_sock_;
    }

protected:
    int port_ = 8080;
    int listen_sock_ = -1;
    static TcpServer* server_;
};

TcpServer* TcpServer::server_ = nullptr;

#endif  // TCP_SERVER_HPP