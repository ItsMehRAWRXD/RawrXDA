// IDELogger Header - Comprehensive Logging System for RawrXD IDE

#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>

// Pull in canonical LogLevel + RAWRXD_LOG_* from src/logging/Logger.h.
// Do NOT use "logging/Logger.h" alone: -Iinclude is ordered before -Isrc on MSVC, so that
// would pick include/logging/logger.h (different API, no RAWRXD_LOG_* macros).
#include "../logging/Logger.h"
namespace RawrXD { namespace Logging { enum class LogLevel; } }
using IDELogLevel = RawrXD::Logging::LogLevel;

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
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            // Idempotent: if already open (first call succeeded), ignore re-init.
            // This prevents the background boot thread from closing the UI thread's
            // log handle and generating a spurious "Logger init failed" event.
            if (m_logFile.is_open()) {
                return;
            }
            m_logFile.open(logPath, std::ios::out | std::ios::app);
            m_initialized = m_logFile.is_open();
            if (m_initialized) {
                writeUnlocked(Level::INFO, "IDELogger", "Logging system initialized");
            }
        }
    }

    void setLevel(Level level) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_minLevel = level;
    }

    void log(Level level, const std::string& function, const std::string& message) {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_initialized || level < m_minLevel) return;

        writeUnlocked(level, function, message);
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
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_logFile.is_open()) {
            writeUnlocked(Level::INFO, "IDELogger", "Logging system shutdown");
            m_logFile.close();
        }
    }

private:
    IDELogger() : m_initialized(false), m_minLevel(Level::TRACE) {}
    IDELogger(const IDELogger&) = delete;
    IDELogger& operator=(const IDELogger&) = delete;

    void writeUnlocked(Level level, const std::string& function, const std::string& message) {
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
