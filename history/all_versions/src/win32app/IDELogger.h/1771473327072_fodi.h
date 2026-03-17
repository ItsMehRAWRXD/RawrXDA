#pragma once
#include <string>
#include <mutex>

#if __has_include(<spdlog/spdlog.h>)
#include <spdlog/spdlog.h>
#endif

// NOTE: Keep IDE logger level distinct from the global LogLevel used by include/logging/logger.h
// to avoid ODR/type redefinition errors when both headers are included.
enum class IDELogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERR,
    CRITICAL
};

class IDELogger {
public:
    static IDELogger& getInstance();

    // Optional init hook (Win32IDE calls this during deferredHeavyInit)
    void initialize(const std::string& logFilePath);

    void log(IDELogLevel level, const std::string& message);

private:
    IDELogger() = default;
    IDELogger(const IDELogger&) = delete;
    IDELogger& operator=(const IDELogger&) = delete;
};

// Convenience logging macros
#ifndef LOG_INFO
#define LOG_INFO(msg) IDELogger::getInstance().log(IDELogLevel::INFO, msg)
#endif
#ifndef LOG_ERROR
#define LOG_ERROR(msg) IDELogger::getInstance().log(IDELogLevel::ERR, msg)
#endif
#ifndef LOG_WARNING
#define LOG_WARNING(msg) IDELogger::getInstance().log(IDELogLevel::WARNING, msg)
#endif
#ifndef LOG_CRITICAL
#define LOG_CRITICAL(msg) IDELogger::getInstance().log(IDELogLevel::CRITICAL, msg)
#endif
#ifndef LOG_DEBUG
#define LOG_DEBUG(msg) IDELogger::getInstance().log(IDELogLevel::DEBUG, msg)
#endif
