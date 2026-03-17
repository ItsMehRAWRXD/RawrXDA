#pragma once

/**
 * @file logger.h
 * @brief Production-grade Logger with file+console output, log levels, timestamps
 * 
 * 100% Qt-free -- pure C++20/STL only.
 * Thread-safe with mutex protection. Supports {} format placeholders.
 */

#include <string>
#include <fstream>
#include <iostream>
#include <chrono>
#include <sstream>
#include <memory>
#include <mutex>
#include <iomanip>

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    CRITICAL = 4
};

class Logger {
private:
    std::string m_name;
    std::ofstream m_logFile;
    LogLevel m_minLevel = LogLevel::INFO;
    std::mutex m_mutex;
    bool m_enableConsole = true;
    bool m_enableFile = true;

public:
    Logger(const std::string& name, const std::string& logPath = "logs/")
        : m_name(name) {
        std::string filename = logPath + name + ".log";
        if (m_enableFile) {
            m_logFile.open(filename, std::ios::app);
        }
    }

    // Backward-compatible: old code used Logger("prefix") with no path
    explicit Logger() : m_name("LOG"), m_enableFile(false) {}

    ~Logger() {
        if (m_logFile.is_open()) {
            m_logFile.close();
        }
    }

    void setMinLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_minLevel = level;
    }

    void enableConsole(bool enable) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_enableConsole = enable;
    }

    void enableFile(bool enable) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_enableFile = enable;
    }

    template<typename... Args>
    void debug(const std::string& format, Args... args) {
        log(LogLevel::DEBUG, format, args...);
    }

    template<typename... Args>
    void info(const std::string& format, Args... args) {
        log(LogLevel::INFO, format, args...);
    }

    template<typename... Args>
    void warn(const std::string& format, Args... args) {
        log(LogLevel::WARN, format, args...);
    }

    template<typename... Args>
    void error(const std::string& format, Args... args) {
        log(LogLevel::ERROR, format, args...);
    }

    template<typename... Args>
    void critical(const std::string& format, Args... args) {
        log(LogLevel::CRITICAL, format, args...);
    }

    // Backward-compatible: old code used log("message")
    void log(const std::string& message) {
        log(LogLevel::INFO, message);
    }

private:
    template<typename... Args>
    void log(LogLevel level, const std::string& format, Args... args) {
        if (level < m_minLevel) return;

        std::lock_guard<std::mutex> lock(m_mutex);

        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms.count();

        std::string levelStr = getLevelString(level);
        std::string message = formatMessage(format, args...);

        std::string logLine = ss.str() + " [" + levelStr + "] [" + m_name + "] " + message;

        if (m_enableConsole) {
            if (level >= LogLevel::ERROR) {
                std::cerr << logLine << std::endl;
            } else {
                std::cout << logLine << std::endl;
            }
        }

        if (m_enableFile && m_logFile.is_open()) {
            m_logFile << logLine << std::endl;
            m_logFile.flush();
        }
    }

    std::string getLevelString(LogLevel level) const {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARN: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }

    template<typename T>
    std::string toString(T value) {
        return std::to_string(value);
    }

    std::string toString(const std::string& value) {
        return value;
    }

    std::string toString(const char* value) {
        return std::string(value);
    }

    template<typename... Args>
    std::string formatMessage(const std::string& format, Args... args) {
        return simpleFormat(format, args...);
    }

    template<typename T, typename... Rest>
    std::string simpleFormat(const std::string& format, T first, Rest... rest) {
        size_t pos = format.find("{}");
        if (pos != std::string::npos) {
            return format.substr(0, pos) + toString(first) +
                   simpleFormat(format.substr(pos + 2), rest...);
        }
        return format;
    }

    std::string simpleFormat(const std::string& format) {
        return format;
    }
};
