#include <iostream>
#include <memory>
#include <chrono>
#include "log.hpp"
#include "http_server.hpp"
#include "tcp_server.hpp"

int main(int argc, char* argv[]) {
    if (2 != argc) {
        std::cout << "请指明端口\n";
        exit(4);
    }

    int port = std::atoi(argv[1]);

    std::shared_ptr<HttpServer> server{new HttpServer{port}};

    server->Loop();

    return 0;
}