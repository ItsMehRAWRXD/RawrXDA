#pragma once

#include <memory>
#include <fstream>
#include <cstdio>
#include <cstdarg>

class DebugLogger {
public:
    static DebugLogger& getInstance() {
        static DebugLogger instance;  // No file yet - just a stub
        return instance;
    }
    
    void init(const char* path) {
        if (!m_file) {
            m_file = std::make_unique<std::ofstream>(path, std::ios::app);
        }
    }
    
    void log(const char* message) {
        if (m_file && m_file->is_open()) {
            *m_file << message << std::flush;
        }
    }
    
private:
    DebugLogger() = default;
    std::unique_ptr<std::ofstream> m_file;
};

#define DEBUG_LOG(msg) DebugLogger::getInstance().log(msg)
