#ifndef LOG_HPP
#define LOG_HPP

#include <string>
#include <chrono>
#include <iostream>

enum Level{
    INFO = 1,
    WARNING,
    ERROR,
    FATAL
};

#define LOG(level, message) Log(level, message, __FILE__, __LINE__)

static void Log(Level level, std::string messge, std::string file_name, int line) {
    // 获取当前时间点
    std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
  // 转换为时间戳（秒数）
    std::time_t timestamp = std::chrono::system_clock::to_time_t(now);

    std::cout << "[" << level << "] ";
    std::cout << "[" << timestamp << "] ";
    std::cout << "[" << messge << "] ";
    std::cout << "[" << file_name << "] ";
    std::cout << "[" << line << "]\n";
    std::cout << std::endl;
}

#endif