#pragma once

#include <string>
#include <string_view>
#include <format>
#include <chrono>
#include <iostream>
#include <fstream>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <utility>

namespace RawrXD::Registry {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

class Logger {
private:
    static inline Logger* s_instance = nullptr;
    static inline std::mutex s_mutex;
    
    LogLevel m_level = LogLevel::Info;
    std::ofstream m_file;
    bool m_consoleEnabled = true;
    bool m_fileEnabled = false;
    
    Logger() = default;
    
    std::string getCurrentTime() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    
    std::string levelToString(LogLevel level) const {
        switch (level) {
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info: return "INFO";
            case LogLevel::Warning: return "WARN";
            case LogLevel::Error: return "ERROR";
            case LogLevel::Critical: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }
    
    void logInternal(LogLevel level, const std::string& message) {
        if (level < m_level) return;
        
        std::string timestamp = getCurrentTime();
        std::string levelStr = levelToString(level);
        std::string formatted = std::format("[{}] [{}] {}", timestamp, levelStr, message);
        
        std::lock_guard<std::mutex> lock(s_mutex);
        
        if (m_consoleEnabled) {
            std::cout << formatted << std::endl;
        }
        
        if (m_fileEnabled && m_file.is_open()) {
            m_file << formatted << std::endl;
            m_file.flush();
        }
    }
    
public:
    static Logger& instance() {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_instance) {
            s_instance = new Logger();
        }
        return *s_instance;
    }
    
    void setLevel(LogLevel level) { m_level = level; }
    void enableConsole(bool enable) { m_consoleEnabled = enable; }
    void enableFile(const std::string& filename) {
        m_file.open(filename, std::ios::app);
        m_fileEnabled = m_file.is_open();
    }
    
    template<typename... Args>
    void debug(std::string_view format, Args&&... args) {
        logInternal(LogLevel::Debug, std::vformat(format, std::make_format_args(args...)));
    }
    
    template<typename... Args>
    void info(std::string_view format, Args&&... args) {
        logInternal(LogLevel::Info, std::vformat(format, std::make_format_args(args...)));
    }
    
    template<typename... Args>
    void warning(std::string_view format, Args&&... args) {
        logInternal(LogLevel::Warning, std::vformat(format, std::make_format_args(args...)));
    }
    
    template<typename... Args>
    void error(std::string_view format, Args&&... args) {
        logInternal(LogLevel::Error, std::vformat(format, std::make_format_args(args...)));
    }
    
    template<typename... Args>
    void critical(std::string_view format, Args&&... args) {
        logInternal(LogLevel::Critical, std::vformat(format, std::make_format_args(args...)));
    }

    template<typename... Args>
    void debug_runtime(std::string_view format, Args&&... args) {
        logInternal(LogLevel::Debug, std::vformat(format, std::make_format_args(args...)));
    }

    template<typename... Args>
    void info_runtime(std::string_view format, Args&&... args) {
        logInternal(LogLevel::Info, std::vformat(format, std::make_format_args(args...)));
    }

    template<typename... Args>
    void warning_runtime(std::string_view format, Args&&... args) {
        logInternal(LogLevel::Warning, std::vformat(format, std::make_format_args(args...)));
    }

    template<typename... Args>
    void error_runtime(std::string_view format, Args&&... args) {
        logInternal(LogLevel::Error, std::vformat(format, std::make_format_args(args...)));
    }

    template<typename... Args>
    void critical_runtime(std::string_view format, Args&&... args) {
        logInternal(LogLevel::Critical, std::vformat(format, std::make_format_args(args...)));
    }

    void debug(const char* message) { logInternal(LogLevel::Debug, message); }
    void info(const char* message) { logInternal(LogLevel::Info, message); }
    void warning(const char* message) { logInternal(LogLevel::Warning, message); }
    void error(const char* message) { logInternal(LogLevel::Error, message); }
    void critical(const char* message) { logInternal(LogLevel::Critical, message); }
};

} // namespace RawrXD::Registry

