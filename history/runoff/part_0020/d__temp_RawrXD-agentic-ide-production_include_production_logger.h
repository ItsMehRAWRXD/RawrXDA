#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include <chrono>
#include <memory>
#include <unordered_map>

// Production-ready comprehensive logging system
// Implements observability requirements from AI Toolkit production guidelines
class ProductionLogger {
public:
    enum LogLevel {
        LEVEL_DEBUG = 0,
        LEVEL_INFO = 1,
        LEVEL_WARNING = 2,
        LEVEL_ERROR = 3,
        LEVEL_CRITICAL = 4
    };

    enum LogCategory {
        STARTUP,
        GUI,
        PAINT,
        CODE_EDITOR,
        CHAT,
        MENU_ACTIONS,
        FILE_OPS,
        AGENTIC,
        PERFORMANCE,
        ERRORS,
        USER_ACTIONS,
        SYSTEM
    };

    static ProductionLogger& getInstance();
    
    // Core logging functions
    void log(LogLevel level, LogCategory category, const std::string& message, 
             const std::string& function = "", int line = 0);
    
    void logMetric(const std::string& metricName, double value, const std::string& unit = "");
    void logLatency(const std::string& operation, std::chrono::milliseconds duration);
    void logUserAction(const std::string& action, const std::string& context = "");
    void logError(const std::string& error, const std::string& stackTrace = "", 
                  const std::string& context = "");

    // Configuration
    void setLogLevel(LogLevel minLevel) { m_minLogLevel = minLevel; }
    void enableConsoleOutput(bool enable) { m_consoleOutput = enable; }
    void setLogDirectory(const std::string& dir) { m_logDirectory = dir; }

    // File rotation and cleanup
    void rotateLogsIfNeeded();
    void flushAllLogs();

private:
    ProductionLogger();
    ~ProductionLogger();
    
    std::string formatTimestamp() const;
    std::string levelToString(LogLevel level) const;
    std::string categoryToString(LogCategory category) const;
    std::string getLogFileName(LogCategory category) const;
    
    void writeToFile(LogCategory category, const std::string& message);
    void writeToConsole(const std::string& message);
    
    LogLevel m_minLogLevel;
    bool m_consoleOutput;
    std::string m_logDirectory;
    std::mutex m_logMutex;
    std::unordered_map<LogCategory, std::unique_ptr<std::ofstream>> m_logFiles;
    
    // Metrics tracking
    std::chrono::steady_clock::time_point m_startTime;
    size_t m_totalLogEntries;
    std::unordered_map<std::string, size_t> m_errorCounts;
    std::unordered_map<std::string, std::chrono::milliseconds> m_operationLatencies;
};

// Convenience macros for easier logging
#define LOG_DEBUG(category, message) ProductionLogger::getInstance().log(ProductionLogger::LEVEL_DEBUG, category, message, __FUNCTION__, __LINE__)
#define LOG_INFO(category, message) ProductionLogger::getInstance().log(ProductionLogger::LEVEL_INFO, category, message, __FUNCTION__, __LINE__)  
#define LOG_WARNING(category, message) ProductionLogger::getInstance().log(ProductionLogger::LEVEL_WARNING, category, message, __FUNCTION__, __LINE__)
#define LOG_ERROR(category, message) ProductionLogger::getInstance().log(ProductionLogger::LEVEL_ERROR, category, message, __FUNCTION__, __LINE__)
#define LOG_CRITICAL(category, message) ProductionLogger::getInstance().log(ProductionLogger::LEVEL_CRITICAL, category, message, __FUNCTION__, __LINE__)

#define LOG_METRIC(name, value, unit) ProductionLogger::getInstance().logMetric(name, value, unit)
#define LOG_LATENCY(operation, duration) ProductionLogger::getInstance().logLatency(operation, duration)
#define LOG_USER_ACTION(action, context) ProductionLogger::getInstance().logUserAction(action, context)

// Performance timing helper
class PerformanceTimer {
public:
    explicit PerformanceTimer(const std::string& operation) 
        : m_operation(operation), m_start(std::chrono::steady_clock::now()) {}
    
    ~PerformanceTimer() {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start);
        LOG_LATENCY(m_operation, duration);
    }
    
private:
    std::string m_operation;
    std::chrono::steady_clock::time_point m_start;
};

#define PERFORMANCE_TIMER(operation) PerformanceTimer _timer(operation)