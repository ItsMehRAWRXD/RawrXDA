// ============================================================================
// logger.cpp — Logger Implementation (Structured JSON logging stub)
// ============================================================================
// Provides Logger::instance() and log() implementation for tests and builds
// that require telemetry symbols.
//
// Pattern: No exceptions, thread-safe singleton
// ============================================================================

#include "telemetry/logger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdarg>

// ============================================================================
// Global Logger Implementation
// ============================================================================

Logger::Logger() {
    // Initialize if needed
}

Logger::~Logger() {
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
}

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::log(LogLevel level, const std::string& message, const LogData& data) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Format the log entry
    std::string levelStr = "INFO";
    if (level == Debug) levelStr = "DEBUG";
    if (level == Warning) levelStr = "WARN";
    if (level == Error) levelStr = "ERROR";

    std::cout << "[" << levelStr << "] " << message << std::endl;
}

// ============================================================================
// RawrXD::Telemetry::Logger Implementation (Satisfy Linker)
// ============================================================================
namespace RawrXD::Telemetry {

enum class TelemetryLevel : uint8_t {
    Off       = 0,
    Critical  = 1,
    Error     = 2,
    Warning   = 3,
    Info      = 4,
    Debug     = 5,
    Trace     = 6,
};

class Logger {
public:
    static void Log(TelemetryLevel level, const char* fmt, ...) {
        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        // Forward to global logger
        ::Logger::instance().log(::Logger::Info, buffer);
    }
};

} // namespace RawrXD::Telemetry

std::string Logger::formatLogEntry(LogLevel level, const std::string& message, const LogData& data) {
    std::ostringstream oss;
    
    // Timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif
    
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count() << " ";
    
    // Level
    const char* levelStr = "UNKNOWN";
    switch (level) {
        case Debug:   levelStr = "DEBUG"; break;
        case Info:    levelStr = "INFO"; break;
        case Warning: levelStr = "WARN"; break;
        case Error:   levelStr = "ERROR"; break;
    }
    oss << "[" << levelStr << "] ";
    
    // Message
    oss << message;
    
    // Data (simple key=value format)
    if (!data.empty()) {
        oss << " {";
        bool first = true;
        for (const auto& [key, value] : data) {
            if (!first) oss << ", ";
            oss << key << "=\"" << value << "\"";
            first = false;
        }
        oss << "}";
    }
    
    return oss.str();
}

void Logger::rotateLogFile() {
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
    // Simple rotation: just reopen (real implementation would rename files)
    m_logFile.open("rawrxd.log", std::ios::out | std::ios::app);
    m_currentFileSize = 0;
}
