#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <fstream>

#if __has_include(<spdlog/spdlog.h>)
#include <spdlog/spdlog.h>
#endif

class IDELogger {
public:
    enum class Level {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        CRITICAL
    };

    static IDELogger& getInstance();
    void initialize(const std::string& logFilePath = "");
    void setLevel(Level level);
    void log(Level level, const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void critical(const std::string& message);
    void debug(const std::string& func, const std::string& message);
    void info(const std::string& func, const std::string& message);
    void warning(const std::string& func, const std::string& message);
    void error(const std::string& func, const std::string& message);
    void critical(const std::string& func, const std::string& message);

private:
    IDELogger() = default;
    std::string m_logFile;
    Level m_level = Level::INFO;
    std::mutex m_mutex;
    std::ofstream m_logStream;
};

// Convenience logging macros
#ifndef LOG_INFO
#define LOG_INFO(msg) IDELogger::getInstance().log(IDELogger::Level::INFO, msg)
#endif
#ifndef LOG_ERROR
#define LOG_ERROR(msg) IDELogger::getInstance().log(IDELogger::Level::ERROR, msg)
#endif
#ifndef LOG_WARNING
#define LOG_WARNING(msg) IDELogger::getInstance().log(IDELogger::Level::WARNING, msg)
#endif
#ifndef LOG_CRITICAL
#define LOG_CRITICAL(msg) IDELogger::getInstance().log(IDELogger::Level::CRITICAL, msg)
#endif
#ifndef LOG_DEBUG
#define LOG_DEBUG(msg) IDELogger::getInstance().log(IDELogger::Level::DEBUG, msg)
#endif
