#pragma once
#include <iostream>
#include <string>

// Simple debug logger stub
class DebugLogger {
public:
    static void log(const std::string& msg) {
        std::cout << "[DEBUG] " << msg << std::endl;
    }
};

#define LOG_DEBUG(msg) DebugLogger::log(msg)
