#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <thread>

#include <signal.h>

#include "protocol.hpp"
#include "tcp_server.hpp"
#include "log.hpp"
#include "task.hpp"
#include "thread_pool.hpp"

static constexpr int PORT = 8080;

class HttpServer{
public:
    HttpServer(int port = PORT) : port_(port), stop(false) {
        signal(SIGPIPE, SIG_IGN);
    }

    void Loop() {
        LOG(INFO, "Loop begin");
        // int listen_sock = tcp_server_->GetSock();
        int listen_sock = TcpServer::GetInstance(port_)->GetSock();

        while (!stop) {
            struct sockaddr_in peer;
            socklen_t len = sizeof(peer);
            memset(&peer, 0, len);
            int sock = accept(listen_sock, (struct sockaddr*)&peer, &len);
            if (sock < 0) {
                continue;
            }
            LOG(INFO, "get a new link");

            // ThreadPool::GetInstance()->PushTask(Task());

            CallBack cb;
            std::thread t(&CallBack::HandlerRequest, &cb, sock);
            t.detach();
        }
    }

protected:
    int port_;
    bool stop;
};

#endif  // HTTP_SERVER_HPP