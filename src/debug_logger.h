#pragma once
#include <string>
#include "logging/logger.h"

// Debug logger facade wrapping the centralized Logger
class DebugLogger {
public:
    static void log(const std::string& msg) {
        static Logger s_logger("DebugLogger");
        s_logger.debug(msg);
    }
};

#define LOG_DEBUG(msg) DebugLogger::log(msg)
