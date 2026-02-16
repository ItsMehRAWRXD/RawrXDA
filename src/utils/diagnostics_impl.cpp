#include "../utils/Diagnostics.hpp"
#include <iostream>
#include <mutex>

#include "logging/logger.h"
static Logger s_logger("diagnostics_impl");

// Define static members
std::ofstream Diagnostics::s_logFile;
Diagnostics::LogLevel Diagnostics::s_minLevel = Diagnostics::LogLevel::Info;
bool Diagnostics::s_initialized = false;
static std::mutex g_logMutex;

void Diagnostics::initialize(const std::string& logFilePath, LogLevel minLevel) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (s_initialized) return;

    s_logFile.open(logFilePath, std::ios::app);
    s_minLevel = minLevel;
    s_initialized = true;
    
    // Log start of session
    if (s_logFile.is_open()) {
        s_logFile << "\n=== RawrXD Diagnostic Session Started " << getCurrentTimestamp() << " ===\n";
    }
}

void Diagnostics::shutdown() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (s_logFile.is_open()) {
        s_logFile << "=== Session Ended ===\n";
        s_logFile.close();
    }
    s_initialized = false;
}

void Diagnostics::log(LogLevel level, const std::string& message, const std::string& component) {
    if (level < s_minLevel) return;

    std::lock_guard<std::mutex> lock(g_logMutex);
    
    std::string timestamp = getCurrentTimestamp();
    std::string levelStr = logLevelToString(level);
    std::string compStr = component.empty() ? "" : "[" + component + "] ";
    
    // Console output
    std::ostream& out = (level >= LogLevel::Error) ? std::cerr : std::cout;
    out << timestamp << " " << levelStr << " " << compStr << message << std::endl;

    // File output
    if (s_initialized && s_logFile.is_open()) {
        s_logFile << timestamp << " " << levelStr << " " << compStr << message << std::endl;
        s_logFile.flush();
    }
}

void Diagnostics::debug(const std::string& message, const std::string& component) {
    log(LogLevel::Debug, message, component);
}

void Diagnostics::info(const std::string& message, const std::string& component) {
    log(LogLevel::Info, message, component);
}

void Diagnostics::warning(const std::string& message, const std::string& component) {
    log(LogLevel::Warning, message, component);
}

void Diagnostics::error(const std::string& message, const std::string& component) {
    log(LogLevel::Error, message, component);
}

void Diagnostics::critical(const std::string& message, const std::string& component) {
    log(LogLevel::Critical, message, component);
}

void Diagnostics::setMinLogLevel(LogLevel level) {
    s_minLevel = level;
}

std::string Diagnostics::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "[%Y-%m-%d %H:%M:%S]");
    return ss.str();
}

std::string Diagnostics::logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:    return "[DEBUG]";
        case LogLevel::Info:     return "[INFO] ";
        case LogLevel::Warning:  return "[WARN] ";
        case LogLevel::Error:    return "[ERROR]";
        case LogLevel::Critical: return "[CRIT] ";
        default:                 return "[UNK]  ";
    }
}
