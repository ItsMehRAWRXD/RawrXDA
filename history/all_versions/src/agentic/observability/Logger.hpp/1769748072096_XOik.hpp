#pragma once

#include <string>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
#include <cstdint>

#ifdef ERROR
#undef ERROR
#endif

namespace RawrXD::Agentic::Observability {

/// Log level
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    FATAL = 4
};

/// Log entry
struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string category;
    std::string message;
    std::string file;
    int line = 0;
    std::string function;
    uint32_t threadId = 0;
    
    std::string toString() const;
};

/// Structured logging engine (per AITK instructions)
class Logger {
public:
    static Logger& instance();
    
    /// Initialize logging subsystem
    bool initialize(const std::string& appName, const std::string& logDir = "logs");
    
    /// Shutdown logging
    void shutdown();
    
    /// Log message
    void log(LogLevel level, const std::string& category, const std::string& message,
             const char* file = nullptr, int line = 0, const char* function = nullptr);
    
    /// Set minimum log level
    void setMinLevel(LogLevel level) { m_minLevel = level; }
    
    /// Set log file path
    bool setLogFile(const std::string& filePath);
    
    /// Enable/disable console output
    void setConsoleOutput(bool enabled) { m_consoleOutput = enabled; }
    
    /// Enable/disable file output
    void setFileOutput(bool enabled) { m_fileOutput = enabled; }
    
    /// Flush logs to disk
    void flush();
    
    /// Get recent logs
    std::vector<LogEntry> getRecentLogs(size_t count = 100) const;
    
    /// Get logs by level
    std::vector<LogEntry> getLogsByLevel(LogLevel level, size_t maxCount = 1000) const;
    
    /// Clear log buffer
    void clear();
    
private:
    Logger() = default;
    ~Logger() { shutdown(); }
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    mutable std::mutex m_mutex;
    std::string m_appName;
    std::string m_logDir;
    std::ofstream m_logFile;
    bool m_consoleOutput = true;
    bool m_fileOutput = true;
    LogLevel m_minLevel = LogLevel::DEBUG;
    
    // Ring buffer for recent logs
    std::vector<LogEntry> m_recentLogs;
    static constexpr size_t MAX_RECENT_LOGS = 1000;
    
    void writeToFile(const LogEntry& entry);
    void writeToConsole(const LogEntry& entry);
};

/// Helper macros
#define LOG_DEBUG(category, message) \
    RawrXD::Agentic::Observability::Logger::instance().log( \
        RawrXD::Agentic::Observability::LogLevel::DEBUG, category, message, __FILE__, __LINE__, __FUNCTION__)

#define LOG_INFO(category, message) \
    RawrXD::Agentic::Observability::Logger::instance().log( \
        RawrXD::Agentic::Observability::LogLevel::INFO, category, message, __FILE__, __LINE__, __FUNCTION__)

#define LOG_WARNING(category, message) \
    RawrXD::Agentic::Observability::Logger::instance().log( \
        RawrXD::Agentic::Observability::LogLevel::WARNING, category, message, __FILE__, __LINE__, __FUNCTION__)

#define LOG_ERROR(category, message) \
    RawrXD::Agentic::Observability::Logger::instance().log( \
        RawrXD::Agentic::Observability::LogLevel::ERROR, category, message, __FILE__, __LINE__, __FUNCTION__)

#define LOG_FATAL(category, message) \
    RawrXD::Agentic::Observability::Logger::instance().log( \
        RawrXD::Agentic::Observability::LogLevel::FATAL, category, message, __FILE__, __LINE__, __FUNCTION__)

} // namespace RawrXD::Agentic::Observability
