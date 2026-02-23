#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>

// Comprehensive logging system for RawrXD IDE
class IDELogger {
public:
    enum class Level {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARNING = 3,
        ERR = 4,
        CRITICAL = 5
    };

    static IDELogger& getInstance() {
        static IDELogger instance;
        return instance;
    }

    void initialize(const std::string& logPath = "RawrXD_IDE.log") {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_logFile.is_open()) {
            m_logFile.close();
        }
        m_logFile.open(logPath, std::ios::out | std::ios::app);
        m_initialized = true;
        log(Level::INFO, "IDELogger", "Logging system initialized");
    }

    void setLevel(Level level) {
        m_minLevel = level;
    }

    void log(Level level, const std::string& function, const std::string& message) {
        if (!m_initialized || level < m_minLevel) return;

        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::tm tm_buf;
        localtime_s(&tm_buf, &time);

        if (m_logFile.is_open()) {
            m_logFile << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
                     << "." << std::setfill('0') << std::setw(3) << ms.count()
                     << " [" << levelToString(level) << "] "
                     << "[" << function << "] "
                     << message << std::endl;
            m_logFile.flush();
        }
    }

    void trace(const std::string& function, const std::string& message) {
        log(Level::TRACE, function, message);
    }

    void debug(const std::string& function, const std::string& message) {
        log(Level::DEBUG, function, message);
    }

    void info(const std::string& function, const std::string& message) {
        log(Level::INFO, function, message);
    }

    void warning(const std::string& function, const std::string& message) {
        log(Level::WARNING, function, message);
    }

    void error(const std::string& function, const std::string& message) {
        log(Level::ERR, function, message);
    }

    void critical(const std::string& function, const std::string& message) {
        log(Level::CRITICAL, function, message);
    }

    ~IDELogger() {
        if (m_logFile.is_open()) {
            log(Level::INFO, "IDELogger", "Logging system shutdown");
            m_logFile.close();
        }
    }

private:
    IDELogger() : m_initialized(false), m_minLevel(Level::TRACE) {}
    IDELogger(const IDELogger&) = delete;
    IDELogger& operator=(const IDELogger&) = delete;

    std::string levelToString(Level level) {
        switch (level) {
            case Level::TRACE: return "TRACE";
            case Level::DEBUG: return "DEBUG";
            case Level::INFO: return "INFO ";
            case Level::WARNING: return "WARN ";
            case Level::ERR: return "ERROR";
            case Level::CRITICAL: return "CRIT ";
            default: return "UNKNOWN";
        }
    }

    std::ofstream m_logFile;
    std::mutex m_mutex;
    bool m_initialized;
    Level m_minLevel;
};

// Convenience macros for logging
#define LOG_TRACE(msg) IDELogger::getInstance().trace(__FUNCTION__, msg)
#define LOG_DEBUG(msg) IDELogger::getInstance().debug(__FUNCTION__, msg)
#define LOG_INFO(msg) IDELogger::getInstance().info(__FUNCTION__, msg)
#define LOG_WARNING(msg) IDELogger::getInstance().warning(__FUNCTION__, msg)
#define LOG_ERROR(msg) IDELogger::getInstance().error(__FUNCTION__, msg)
#define LOG_CRITICAL(msg) IDELogger::getInstance().critical(__FUNCTION__, msg)
