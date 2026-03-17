// ============================================================================
// logger.cpp — Logger Implementation (Structured JSON logging stub)
// ============================================================================
// Provides Logger::instance() and log() implementation for tests and builds
// that require telemetry symbols.
//
// Pattern: No exceptions, thread-safe singleton
// ============================================================================

#include "telemetry/logger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

Logger::Logger() {
    // Initialize if needed
}

Logger::~Logger() {
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
}

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::log(LogLevel level, const std::string& message, const LogData& data) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Format the log entry
    std::string entry = formatLogEntry(level, message, data);
    
    // Write to stdout
    std::cout << entry << std::endl;
    
    // Write to file if enabled
    if (m_logFile.is_open()) {
        m_logFile << entry << std::endl;
        m_currentFileSize += static_cast<int>(entry.length()) + 1;
        
        if (m_currentFileSize > m_maxFileSize) {
            rotateLogFile();
        }
    }
}

std::string Logger::formatLogEntry(LogLevel level, const std::string& message, const LogData& data) {
    std::ostringstream oss;
    
    // Timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif
    
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count() << " ";
    
    // Level
    const char* levelStr = "UNKNOWN";
    switch (level) {
        case Debug:   levelStr = "DEBUG"; break;
        case Info:    levelStr = "INFO"; break;
        case Warning: levelStr = "WARN"; break;
        case Error:   levelStr = "ERROR"; break;
    }
    oss << "[" << levelStr << "] ";
    
    // Message
    oss << message;
    
    // Data (simple key=value format)
    if (!data.empty()) {
        oss << " {";
        bool first = true;
        for (const auto& [key, value] : data) {
            if (!first) oss << ", ";
            oss << key << "=\"" << value << "\"";
            first = false;
        }
        oss << "}";
    }
    
    return oss.str();
}

void Logger::rotateLogFile() {
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
    // Simple rotation: just reopen (real implementation would rename files)
    m_logFile.open("rawrxd.log", std::ios::out | std::ios::app);
    m_currentFileSize = 0;
}
