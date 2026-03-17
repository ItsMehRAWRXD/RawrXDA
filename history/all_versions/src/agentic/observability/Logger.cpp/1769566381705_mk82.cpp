#include "Logger.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <windows.h>

namespace RawrXD::Agentic::Observability {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

bool Logger::initialize(const std::string& appName, const std::string& logDir) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_appName = appName;
    m_logDir = logDir;
    
    // Create log directory
    CreateDirectoryA(logDir.c_str(), nullptr);
    
    // Open log file
    std::string logPath = logDir + "/" + appName + ".log";
    return setLogFile(logPath);
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logFile.is_open()) {
        flush();
        m_logFile.close();
    }
}

void Logger::log(LogLevel level, const std::string& category, const std::string& message,
                const char* file, int line, const char* function) {
    if (level < m_minLevel) {
        return;
    }
    
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = level;
    entry.category = category;
    entry.message = message;
    if (file) entry.file = file;
    entry.line = line;
    if (function) entry.function = function;
    entry.threadId = GetCurrentThreadId();
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Add to ring buffer
    if (m_recentLogs.size() >= MAX_RECENT_LOGS) {
        m_recentLogs.erase(m_recentLogs.begin());
    }
    m_recentLogs.push_back(entry);
    
    // Output
    if (m_consoleOutput) {
        writeToConsole(entry);
    }
    if (m_fileOutput && m_logFile.is_open()) {
        writeToFile(entry);
    }
}

bool Logger::setLogFile(const std::string& filePath) {
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
    
    m_logFile.open(filePath, std::ios::out | std::ios::app);
    return m_logFile.is_open();
}

void Logger::flush() {
    if (m_logFile.is_open()) {
        m_logFile.flush();
    }
}

std::vector<LogEntry> Logger::getRecentLogs(size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t start = (m_recentLogs.size() > count) ? m_recentLogs.size() - count : 0;
    return std::vector<LogEntry>(m_recentLogs.begin() + start, m_recentLogs.end());
}

std::vector<LogEntry> Logger::getLogsByLevel(LogLevel level, size_t maxCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<LogEntry> filtered;
    
    for (const auto& entry : m_recentLogs) {
        if (entry.level == level) {
            filtered.push_back(entry);
            if (filtered.size() >= maxCount) {
                break;
            }
        }
    }
    
    return filtered;
}

void Logger::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_recentLogs.clear();
}

void Logger::writeToFile(const LogEntry& entry) {
    m_logFile << entry.toString() << "\n";
}

void Logger::writeToConsole(const LogEntry& entry) {
    std::cout << entry.toString() << std::endl;
}

std::string LogEntry::toString() const {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::tm tm;
    localtime_s(&tm, &time_t);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " ";
    
    switch (level) {
        case LogLevel::DEBUG:   oss << "[DEBUG] "; break;
        case LogLevel::INFO:    oss << "[INFO]  "; break;
        case LogLevel::WARNING: oss << "[WARN]  "; break;
        case LogLevel::ERROR:   oss << "[ERROR] "; break;
        case LogLevel::FATAL:   oss << "[FATAL] "; break;
    }
    
    oss << "[" << category << "] ";
    oss << message;
    
    if (!file.empty() || line > 0 || !function.empty()) {
        oss << " (";
        if (!file.empty()) oss << file;
        if (line > 0) oss << ":" << line;
        if (!function.empty()) oss << " in " << function;
        oss << ")";
    }
    
    return oss.str();
}

} // namespace RawrXD::Agentic::Observability
