#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <thread>

#include "protocol.hpp"
#include "tcp_server.hpp"
#include "log.hpp"

static constexpr int PORT = 8080;

class HttpServer{
public:
    HttpServer(int port = PORT) : port_(port), tcp_server_(nullptr), stop(false) {
        tcp_server_ = TcpServer::GetInstance(port_);
    }

    void Loop() {
        LOG(INFO, "Loop begin");
        int listen_sock = tcp_server_->GetSock();

        while (!stop) {
            struct sockaddr_in peer;
            socklen_t len = sizeof(peer);
            memset(&peer, 0, len);
            int sock = accept(listen_sock, (struct sockaddr*)&peer, &len);
            if (sock < 0) {
                continue;
            }
            LOG(INFO, "get a new link");
            std::thread t(Entrance::HandlerRequest, sock);
            t.detach();
        }
    }

protected:
    int port_;
    TcpServer* tcp_server_;
    bool stop;
};

#endif  // HTTP_SERVER_HPP