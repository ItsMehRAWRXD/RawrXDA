#include "production_logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>

ProductionLogger& ProductionLogger::getInstance() {
    static ProductionLogger instance;
    return instance;
}

ProductionLogger::ProductionLogger() 
    : m_minLogLevel(LEVEL_INFO)
    , m_consoleOutput(true)
    , m_logDirectory("logs")
    , m_startTime(std::chrono::steady_clock::now())
    , m_totalLogEntries(0) {
    
    // Create logs directory if it doesn't exist
    std::filesystem::create_directories(m_logDirectory);
    
    // Initialize log files for each category
    for (int cat = STARTUP; cat <= SYSTEM; ++cat) {
        LogCategory category = static_cast<LogCategory>(cat);
        std::string filename = getLogFileName(category);
        m_logFiles[category] = std::make_unique<std::ofstream>(filename, std::ios::app);
        
        if (m_logFiles[category]->is_open()) {
            *m_logFiles[category] << "\n=== NEW SESSION " << formatTimestamp() << " ===\n";
            m_logFiles[category]->flush();
        }
    }
    
    // Log system startup
    log(LEVEL_INFO, STARTUP, "Production Logger initialized - comprehensive logging enabled");
}

ProductionLogger::~ProductionLogger() {
    flushAllLogs();
    
    // Log session statistics
    auto sessionDuration = std::chrono::steady_clock::now() - m_startTime;
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(sessionDuration);
    
    std::string statsMessage = "SESSION END - Duration: " + std::to_string(minutes.count()) + 
                              "min, Total log entries: " + std::to_string(m_totalLogEntries);
    log(LEVEL_INFO, SYSTEM, statsMessage);
    
    flushAllLogs();
}

void ProductionLogger::log(LogLevel level, LogCategory category, const std::string& message, 
                          const std::string& function, int line) {
    if (level < m_minLogLevel) return;
    
    std::lock_guard<std::mutex> lock(m_logMutex);
    
    std::stringstream ss;
    ss << "[" << formatTimestamp() << "] "
       << "[" << levelToString(level) << "] "
       << "[" << categoryToString(category) << "] ";
    
    if (!function.empty() && line > 0) {
        ss << "[" << function << ":" << line << "] ";
    }
    
    ss << message;
    
    std::string logEntry = ss.str();
    
    // Write to category-specific file
    writeToFile(category, logEntry);
    
    // Also write to main combined log
    writeToFile(SYSTEM, logEntry);
    
    // Console output for real-time monitoring
    if (m_consoleOutput) {
        writeToConsole(logEntry);
    }
    
    m_totalLogEntries++;
    
    // Track error counts for observability
    if (level == LEVEL_ERROR || level == LEVEL_CRITICAL) {
        m_errorCounts[categoryToString(category)]++;
    }
}

void ProductionLogger::logMetric(const std::string& metricName, double value, const std::string& unit) {
    std::stringstream ss;
    ss << "METRIC: " << metricName << " = " << value;
    if (!unit.empty()) ss << " " << unit;
    
    log(LEVEL_INFO, PERFORMANCE, ss.str());
}

void ProductionLogger::logLatency(const std::string& operation, std::chrono::milliseconds duration) {
    m_operationLatencies[operation] = duration;
    
    std::stringstream ss;
    ss << "LATENCY: " << operation << " took " << duration.count() << "ms";
    
    // Log as warning if operation is slow (>1000ms)
    LogLevel level = (duration.count() > 1000) ? LEVEL_WARNING : LEVEL_INFO;
    log(level, PERFORMANCE, ss.str());
}

void ProductionLogger::logUserAction(const std::string& action, const std::string& context) {
    std::stringstream ss;
    ss << "USER_ACTION: " << action;
    if (!context.empty()) ss << " - Context: " << context;
    
    log(LEVEL_INFO, USER_ACTIONS, ss.str());
}

void ProductionLogger::logError(const std::string& error, const std::string& stackTrace, 
                               const std::string& context) {
    std::stringstream ss;
    ss << "ERROR: " << error;
    if (!context.empty()) ss << " - Context: " << context;
    if (!stackTrace.empty()) ss << "\nStack Trace:\n" << stackTrace;
    
    log(LEVEL_ERROR, ERRORS, ss.str());
}

std::string ProductionLogger::formatTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

std::string ProductionLogger::levelToString(LogLevel level) const {
    switch (level) {
        case LEVEL_DEBUG: return "DEBUG";
        case LEVEL_INFO: return "INFO ";
        case LEVEL_WARNING: return "WARN ";
        case LEVEL_ERROR: return "ERROR";
        case LEVEL_CRITICAL: return "CRIT ";
        default: return "UNKN ";
    }
}

std::string ProductionLogger::categoryToString(LogCategory category) const {
    switch (category) {
        case STARTUP: return "STARTUP";
        case GUI: return "GUI";
        case PAINT: return "PAINT";
        case CODE_EDITOR: return "CODE";
        case CHAT: return "CHAT";
        case MENU_ACTIONS: return "MENU";
        case FILE_OPS: return "FILE";
        case AGENTIC: return "AGENT";
        case PERFORMANCE: return "PERF";
        case ERRORS: return "ERROR";
        case USER_ACTIONS: return "USER";
        case SYSTEM: return "SYSTEM";
        default: return "UNKNOWN";
    }
}

std::string ProductionLogger::getLogFileName(LogCategory category) const {
    std::string baseName = m_logDirectory + "/";
    
    switch (category) {
        case STARTUP: return baseName + "startup.log";
        case GUI: return baseName + "gui.log";
        case PAINT: return baseName + "paint.log";
        case CODE_EDITOR: return baseName + "code_editor.log";
        case CHAT: return baseName + "chat.log";
        case MENU_ACTIONS: return baseName + "menu_actions.log";
        case FILE_OPS: return baseName + "file_operations.log";
        case AGENTIC: return baseName + "agentic.log";
        case PERFORMANCE: return baseName + "performance.log";
        case ERRORS: return baseName + "errors.log";
        case USER_ACTIONS: return baseName + "user_actions.log";
        case SYSTEM: return baseName + "system_combined.log";
        default: return baseName + "unknown.log";
    }
}

void ProductionLogger::writeToFile(LogCategory category, const std::string& message) {
    auto it = m_logFiles.find(category);
    if (it != m_logFiles.end() && it->second->is_open()) {
        *it->second << message << std::endl;
        it->second->flush(); // Ensure immediate write for production debugging
    }
}

void ProductionLogger::writeToConsole(const std::string& message) {
    std::cout << message << std::endl;
}

void ProductionLogger::rotateLogsIfNeeded() {
    // Check file sizes and rotate if they exceed 10MB
    for (auto& pair : m_logFiles) {
        std::string filename = getLogFileName(pair.first);
        
        try {
            if (std::filesystem::file_size(filename) > 10 * 1024 * 1024) { // 10MB
                pair.second->close();
                
                // Rename current log to .old
                std::string oldFile = filename + ".old";
                std::filesystem::rename(filename, oldFile);
                
                // Create new log file
                pair.second = std::make_unique<std::ofstream>(filename, std::ios::app);
                
                log(LEVEL_INFO, SYSTEM, "Log rotation completed for " + filename);
            }
        } catch (const std::exception& e) {
            // Error handling without recursive logging
            std::cerr << "Log rotation failed: " << e.what() << std::endl;
        }
    }
}

void ProductionLogger::flushAllLogs() {
    std::lock_guard<std::mutex> lock(m_logMutex);
    for (auto& pair : m_logFiles) {
        if (pair.second->is_open()) {
            pair.second->flush();
        }
    }
}