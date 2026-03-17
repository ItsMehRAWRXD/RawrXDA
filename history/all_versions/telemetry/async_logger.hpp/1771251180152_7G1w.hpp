// telemetry/async_logger.hpp
#pragma once
#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace RawrXD {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

class AsyncLogger {
public:
    static AsyncLogger& instance();
    
    void start();
    void stop();
    
    void log(LogLevel level, const char* message);
    void log(LogLevel level, const std::string& message);
    
private:
    AsyncLogger();
    ~AsyncLogger();
    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;
    
    void workerThread();
    void writeToFile(const std::string& entry);
    
    std::thread worker_;
    std::atomic<bool> running_;
    std::queue<std::string> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    HANDLE hFile_;
};

#define RAWR_LOG_DEBUG(msg) AsyncLogger::instance().log(LogLevel::DEBUG, msg)
#define RAWR_LOG_INFO(msg) AsyncLogger::instance().log(LogLevel::INFO, msg)
#define RAWR_LOG_WARN(msg) AsyncLogger::instance().log(LogLevel::WARN, msg)
#define RAWR_LOG_ERROR(msg) AsyncLogger::instance().log(LogLevel::ERROR, msg)

} // namespace RawrXD