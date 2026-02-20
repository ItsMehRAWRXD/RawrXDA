#pragma once
#include <string>
#include <vector>
#include <mutex>

#if __has_include(<spdlog/spdlog.h>)
#include <spdlog/spdlog.h>
#endif

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERR, // Renamed to avoid macro conflict
    CRITICAL
};

class IDELogger {
public:
    static IDELogger& getInstance();
    void log(LogLevel level, const std::string& message);

private:
    IDELogger() = default;
};

// Convenience logging macros
#ifndef LOG_INFO
#define LOG_INFO(msg) IDELogger::getInstance().log(LogLevel::INFO, msg)
#endif
#ifndef LOG_ERROR
#define LOG_ERROR(msg) IDELogger::getInstance().log(LogLevel::ERR, msg)
#endif
#ifndef LOG_WARNING
#define LOG_WARNING(msg) IDELogger::getInstance().log(LogLevel::WARNING, msg)
#endif
#ifndef LOG_CRITICAL
#define LOG_CRITICAL(msg) IDELogger::getInstance().log(LogLevel::CRITICAL, msg)
#endif
#ifndef LOG_DEBUG
#define LOG_DEBUG(msg) IDELogger::getInstance().log(LogLevel::DEBUG, msg)
#endif
