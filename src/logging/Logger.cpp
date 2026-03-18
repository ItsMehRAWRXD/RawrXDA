#include "Logger.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <sstream>

namespace RawrXD::Logging {

std::string StandardLogFormatter::format(const LogEntry& entry) const {
    std::ostringstream oss;
    auto time = std::chrono::system_clock::to_time_t(entry.timestamp);
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");

    std::string levelStr;
    switch (entry.level) {
        case LogLevel::DEBUG: levelStr = "DEBUG"; break;
        case LogLevel::INFO: levelStr = "INFO"; break;
        case LogLevel::WARNING: levelStr = "WARN"; break;
        case LogLevel::ERROR: levelStr = "ERROR"; break;
        case LogLevel::CRITICAL: levelStr = "CRIT"; break;
    }

    oss << " [" << levelStr << "] [" << entry.component << "] "
        << entry.message;

    return oss.str();
}

std::string StructuredLogFormatter::format(const LogEntry& entry) const {
    std::ostringstream oss;
    auto time = std::chrono::system_clock::to_time_t(entry.timestamp);
    oss << "{"
        << "\"timestamp\":\"" << std::put_time(std::localtime(&time), "%Y-%m-%dT%H:%M:%S") << "\","
        << "\"level\":\"";

    switch (entry.level) {
        case LogLevel::DEBUG: oss << "DEBUG"; break;
        case LogLevel::INFO: oss << "INFO"; break;
        case LogLevel::WARNING: oss << "WARNING"; break;
        case LogLevel::ERROR: oss << "ERROR"; break;
        case LogLevel::CRITICAL: oss << "CRITICAL"; break;
    }

    oss << "\",\"component\":\"" << entry.component << "\","
        << "\"message\":\"" << entry.message << "\","
        << "\"thread\":\"" << entry.threadId << "\""
        << "}";

    return oss.str();
}

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    formatter_ = std::make_unique<StandardLogFormatter>();
}

void Logger::log(LogLevel level, const std::string& message, const std::string& component) {
    if (level < minLevel_) return;

    LogEntry entry{
        level,
        message,
        component,
        std::chrono::system_clock::now(),
        std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()))
    };

    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << formatter_->format(entry) << std::endl;
}

LogStream::LogStream(LogLevel level, const std::string& component, Logger& logger)
    : level_(level), component_(component), logger_(logger) {}

LogStream::~LogStream() {
    logger_.log(level_, stream_.str(), component_);
}

} // namespace RawrXD::Logging