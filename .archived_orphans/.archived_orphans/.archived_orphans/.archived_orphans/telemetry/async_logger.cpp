// telemetry/async_logger.cpp - Simplified synchronous logger
#include "async_logger.hpp"
#include <iostream>

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
    running_ = true;
}

void AsyncLogger::stop() {
    running_ = false;
    if (hFile_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile_);
        hFile_ = INVALID_HANDLE_VALUE;
    }
}

void AsyncLogger::log(LogLevel level, const char* message) {
    if (!running_) return;
    
    const char* levelStr = "UNKNOWN";
    switch (level) {
        case LogLevel::DEBUG: levelStr = "DEBUG"; break;
        case LogLevel::INFO: levelStr = "INFO"; break;
        case LogLevel::WARN: levelStr = "WARN"; break;
        case LogLevel::ERR: levelStr = "ERROR"; break;
    }
    
    std::cout << "[" << levelStr << "] " << message << std::endl;
}

void AsyncLogger::log(LogLevel level, const std::string& message) {
    log(level, message.c_str());
}

void AsyncLogger::workerThread() {
    // No-op
}

void AsyncLogger::writeToFile(const std::string& entry) {
    // No-op
}

} // namespace RawrXD