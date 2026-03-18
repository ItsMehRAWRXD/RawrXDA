#pragma once
#include <string>
#include <sstream>
#include <chrono>
#include <mutex>
#include <memory>
#include <vector>

namespace RawrXD::Logging {

// Windows headers define ERROR as a macro (wingdi.h: #define ERROR 0)
// Save and undefine it so the enum compiles, then restore afterward.
#ifdef ERROR
#  pragma push_macro("ERROR")
#  undef ERROR
#  define RAWRXD_RESTORE_ERROR_MACRO
#endif

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

struct LogEntry {
    LogLevel level;
    std::string message;
    std::string component;
    std::chrono::system_clock::time_point timestamp;
    std::string threadId;
};

class LogFormatter {
public:
    virtual ~LogFormatter() = default;
    virtual std::string format(const LogEntry& entry) const = 0;
};

class StandardLogFormatter : public LogFormatter {
public:
    std::string format(const LogEntry& entry) const override;
};

class StructuredLogFormatter : public LogFormatter {
public:
    std::string format(const LogEntry& entry) const override;
};

class Logger {
public:
    static Logger& instance();

    void setLevel(LogLevel level) { minLevel_ = level; }
    void setFormatter(std::unique_ptr<LogFormatter> formatter) {
        formatter_ = std::move(formatter);
    }

    void log(LogLevel level, const std::string& message,
             const std::string& component = "default");

    // Convenience methods
    void debug(const std::string& message, const std::string& component = "default") {
        log(LogLevel::DEBUG, message, component);
    }
    void info(const std::string& message, const std::string& component = "default") {
        log(LogLevel::INFO, message, component);
    }
    void warning(const std::string& message, const std::string& component = "default") {
        log(LogLevel::WARNING, message, component);
    }
    void error(const std::string& message, const std::string& component = "default") {
        log(LogLevel::ERROR, message, component);
    }
    void critical(const std::string& message, const std::string& component = "default") {
        log(LogLevel::CRITICAL, message, component);
    }

private:
    Logger();

    LogLevel minLevel_{LogLevel::INFO};
    std::unique_ptr<LogFormatter> formatter_;
    std::mutex mutex_;
};

class LogStream {
public:
    LogStream(LogLevel level, const std::string& component, Logger& logger);
    ~LogStream();

    template<typename T>
    LogStream& operator<<(const T& value) {
        stream_ << value;
        return *this;
    }

private:
    LogLevel level_;
    std::string component_;
    Logger& logger_;
    std::ostringstream stream_;
};

} // namespace RawrXD::Logging

// Restore the Windows ERROR macro after the namespace is closed
#ifdef RAWRXD_RESTORE_ERROR_MACRO
#  pragma pop_macro("ERROR")
#  undef RAWRXD_RESTORE_ERROR_MACRO
#endif

// Convenience macros
#define RAWRXD_LOG_DEBUG(component) RawrXD::Logging::LogStream(RawrXD::Logging::LogLevel::DEBUG, component, RawrXD::Logging::Logger::instance())
#define RAWRXD_LOG_INFO(component) RawrXD::Logging::LogStream(RawrXD::Logging::LogLevel::INFO, component, RawrXD::Logging::Logger::instance())
#define RAWRXD_LOG_WARNING(component) RawrXD::Logging::LogStream(RawrXD::Logging::LogLevel::WARNING, component, RawrXD::Logging::Logger::instance())
#define RAWRXD_LOG_ERROR(component) RawrXD::Logging::LogStream(RawrXD::Logging::LogLevel::ERROR, component, RawrXD::Logging::Logger::instance())
#define RAWRXD_LOG_CRITICAL(component) RawrXD::Logging::LogStream(RawrXD::Logging::LogLevel::CRITICAL, component, RawrXD::Logging::Logger::instance())