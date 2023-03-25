#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

#include "task.hpp"

static constexpr int NUM = 10;

class ThreadPool{
public:
    static ThreadPool* GetInstance() {
        static std::mutex mtx;
        if (nullptr == single_instance_) {
            mtx.lock();
            if (nullptr == single_instance_) {
                single_instance_ = new ThreadPool{};
            }
            mtx.unlock();
        }
        LOG(INFO, "return signal instance");
        return single_instance_;
    }

    bool isStop() const {
        return stop_;
    }

    void PushTask(Task task) {
        lock_.lock();
        task_queue_.push(task);
        lock_.unlock();
        cond_.notify_one();
    }

protected:
    ThreadPool(int num = NUM) : num_(num), stop_(false) {
        for (int i = 0; i < num_; ++i) {
            std::thread t(&ThreadPool::ThreadRoutine, this);
            t.detach();
        }
        LOG(INFO, "init thread pool");
    }

    ThreadPool(const ThreadPool&) = delete;

    ThreadPool& operator=(const ThreadPool&) = delete;

    void ThreadRoutine() {
        while (true) {
            Task task;
            lock_.lock();
            std::unique_lock<std::mutex> u_lock;
            while (task_queue_.empty()) {
                cond_.wait(u_lock);
            }
            task = task_queue_.front();
            task_queue_.pop();
            lock_.unlock();
            task.ProcessOn();
        }
    }

protected:
    int num_;
    bool stop_;
    std::queue<Task> task_queue_;
    std::mutex lock_;
    std::condition_variable cond_;

    static ThreadPool* single_instance_;
};

ThreadPool* ThreadPool::single_instance_ = nullptr;

#endif