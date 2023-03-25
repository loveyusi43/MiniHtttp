#ifndef TASK_HPP
#define TASK_HPP

#include "protocol.hpp"

class Task{
public:
    Task() = default;

    Task(int sock) : sockfd_(sock) {}

    void ProcessOn() {
        handler_(sockfd_);
    }

protected:
    int sockfd_ = -1;
    CallBack handler_;
};

#endif