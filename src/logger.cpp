// ============================================================================
// logger.cpp — Production Logger Implementation
// ============================================================================
#include "logger.h"
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>

namespace {
    std::mutex g_logMutex;
    std::ofstream g_logFile;
    bool g_initialized = false;
}

void Logger::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    std::string line = "[" + oss.str() + "] [LOG] " + message;
    std::cout << line << std::endl;
    if (g_initialized && g_logFile.is_open()) {
        g_logFile << line << std::endl;
    }
}

void Logger::error(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    std::string line = "[" + oss.str() + "] [ERROR] " + message;
    std::cerr << line << std::endl;
    if (g_initialized && g_logFile.is_open()) {
        g_logFile << line << std::endl;
    }
}

void Logger::warn(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    std::string line = "[" + oss.str() + "] [WARN] " + message;
    std::cout << line << std::endl;
    if (g_initialized && g_logFile.is_open()) {
        g_logFile << line << std::endl;
    }
}

void Logger::info(const std::string& message) {
    log(message);
}

void Logger::debug(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    std::string line = "[" + oss.str() + "] [DEBUG] " + message;
    std::cout << line << std::endl;
    if (g_initialized && g_logFile.is_open()) {
        g_logFile << line << std::endl;
    }
}

bool Logger::initializeFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_initialized) return true;
    g_logFile.open(path, std::ios::out | std::ios::app);
    g_initialized = g_logFile.is_open();
    return g_initialized;
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile.is_open()) {
        g_logFile.close();
    }
    g_initialized = false;
}
