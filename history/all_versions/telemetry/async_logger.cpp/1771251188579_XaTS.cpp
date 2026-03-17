// telemetry/async_logger.cpp
#include "async_logger.hpp"
#include <chrono>
#include <ctime>

namespace RawrXD {

AsyncLogger& AsyncLogger::instance() {
    static AsyncLogger instance;
    return instance;
}

AsyncLogger::AsyncLogger() : running_(false), hFile_(INVALID_HANDLE_VALUE) {}

AsyncLogger::~AsyncLogger() {
    stop();
}

void AsyncLogger::start() {
    if (running_) return;
    
    // Create log file
    hFile_ = CreateFileW(L"rawrxd.log", GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile_ == INVALID_HANDLE_VALUE) return;
    
    running_ = true;
    worker_ = std::thread(&AsyncLogger::workerThread, this);
}

void AsyncLogger::stop() {
    if (!running_) return;
    
    running_ = false;
    cv_.notify_one();
    if (worker_.joinable()) {
        worker_.join();
    }
    
    if (hFile_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile_);
        hFile_ = INVALID_HANDLE_VALUE;
    }
}

void AsyncLogger::log(LogLevel level, const char* message) {
    if (!running_) return;
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    char timestamp[32];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", 
                  std::localtime(&time));
    
    const char* level_str[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    
    char buffer[1024];
    int len = sprintf_s(buffer, sizeof(buffer), "[%s.%03d] [%s] %s\r\n", 
                       timestamp, static_cast<int>(ms.count()), 
                       level_str[static_cast<int>(level)], message);
    
    if (len > 0) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push(std::string(buffer, len));
        cv_.notify_one();
    }
}

void AsyncLogger::log(LogLevel level, const std::string& message) {
    log(level, message.c_str());
}

void AsyncLogger::workerThread() {
    while (running_ || !queue_.empty()) {
        std::string entry;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return !queue_.empty() || !running_; });
            
            if (!running_ && queue_.empty()) break;
            
            if (!queue_.empty()) {
                entry = std::move(queue_.front());
                queue_.pop();
            }
        }
        
        if (!entry.empty()) {
            writeToFile(entry);
        }
    }
}

void AsyncLogger::writeToFile(const std::string& entry) {
    if (hFile_ == INVALID_HANDLE_VALUE) return;
    
    DWORD written;
    WriteFile(hFile_, entry.c_str(), static_cast<DWORD>(entry.size()), 
              &written, nullptr);
}

} // namespace RawrXD