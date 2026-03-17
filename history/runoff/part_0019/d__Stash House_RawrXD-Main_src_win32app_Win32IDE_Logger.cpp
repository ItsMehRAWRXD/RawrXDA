#include "IDELogger.h"
#include <iostream>
#include <ctime>

IDELogger& IDELogger::getInstance() {
    static IDELogger instance;
    return instance;
}

void IDELogger::initialize(const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logFile = logFilePath;
    if (!m_logFile.empty()) {
        m_logStream.open(m_logFile, std::ios::app);
    }
}

void IDELogger::setLevel(Level level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_level = level;
}

void IDELogger::log(Level level, const std::string& message) {
    if (level < m_level) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string levelStr;
    switch (level) {
        case Level::DEBUG: levelStr = "DEBUG"; break;
        case Level::INFO: levelStr = "INFO"; break;
        case Level::WARNING: levelStr = "WARNING"; break;
        case Level::ERROR: levelStr = "ERROR"; break;
        case Level::CRITICAL: levelStr = "CRITICAL"; break;
    }
    std::time_t now = std::time(nullptr);
    char timeStr[20];
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    std::string logMsg = std::string(timeStr) + " [" + levelStr + "] " + message + "\n";
    if (m_logStream.is_open()) {
        m_logStream << logMsg;
        m_logStream.flush();
    }
    std::cout << logMsg;
}

void IDELogger::debug(const std::string& message) {
    log(Level::DEBUG, message);
}

void IDELogger::info(const std::string& message) {
    log(Level::INFO, message);
}

void IDELogger::warning(const std::string& message) {
    log(Level::WARNING, message);
}

void IDELogger::error(const std::string& message) {
    log(Level::ERROR, message);
}

void IDELogger::critical(const std::string& message) {
    log(Level::CRITICAL, message);
}

void IDELogger::debug(const std::string& func, const std::string& message) {
    log(Level::DEBUG, func + ": " + message);
}

void IDELogger::info(const std::string& func, const std::string& message) {
    log(Level::INFO, func + ": " + message);
}

void IDELogger::warning(const std::string& func, const std::string& message) {
    log(Level::WARNING, func + ": " + message);
}

void IDELogger::error(const std::string& func, const std::string& message) {
    log(Level::ERROR, func + ": " + message);
}

void IDELogger::critical(const std::string& func, const std::string& message) {
    log(Level::CRITICAL, func + ": " + message);
}