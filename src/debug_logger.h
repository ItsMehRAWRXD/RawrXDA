#pragma once
// Consolidated debug_logger.h — combines the static-method API from src/ and
// the singleton/init API from include/ so both call styles work everywhere.
// Delegates everything to the centralized Logger in logging/logger.h.
#include "logging/logger.h"
#include <string>

class DebugLogger {
public:
    // Singleton accessor (from include/ version)
    static DebugLogger& getInstance() {
        static DebugLogger instance;
        return instance;
    }

    // Init kept for API compatibility; actual init handled by Logger
    void init(const char* /*path*/) {}

    // Instance method (from include/ version — accepts const char*)
    void log(const char* message) {
        m_logger.debug(message);
    }

    // Static convenience method (from src/ version — accepts std::string)
    static void log(const std::string& msg) {
        getInstance().m_logger.debug(msg);
    }

private:
    DebugLogger() : m_logger("DebugLogger") {}
    Logger m_logger;
};

// Both macro styles work
#define LOG_DEBUG(msg) DebugLogger::log(msg)
#define DEBUG_LOG(msg) DebugLogger::getInstance().log(msg)
