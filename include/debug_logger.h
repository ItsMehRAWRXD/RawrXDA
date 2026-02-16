#pragma once

#include "logging/logger.h"

// DebugLogger facade that delegates to the centralized Logger.
// Replaces the old file-based stub with structured logging.
class DebugLogger {
public:
    static DebugLogger& getInstance() {
        static DebugLogger instance;
        return instance;
    }
    
    void init(const char* /*path*/) {
        // Initialization is handled by the centralized Logger.
        // The path parameter is retained for API compatibility.
    }
    
    void log(const char* message) {
        m_logger.debug(message);
    }
    
private:
    DebugLogger() : m_logger("DebugLogger") {}
    Logger m_logger;
};

#define DEBUG_LOG(msg) DebugLogger::getInstance().log(msg)
