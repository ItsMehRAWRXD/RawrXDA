#include "logger.h"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>

Logger::Logger()
    : m_maxFileSize(10 * 1024 * 1024) // 10 MB
    , m_currentFileSize(0)
{
    // Create logs directory if it doesn't exist
    std::error_code ec;
    std::filesystem::path logDir("logs");
    if (!std::filesystem::exists(logDir, ec)) {
        std::filesystem::create_directories(logDir, ec);
    }

    // Open log file in append mode
    m_logFile.open("logs/app.log", std::ios::out | std::ios::app);
    if (!m_logFile.is_open()) {
        fprintf(stderr, "[WARN] Failed to open log file: logs/app.log\n");
        return;
    }

    // Get current file size
    std::error_code sizeEc;
    auto fileSize = std::filesystem::file_size("logs/app.log", sizeEc);
    if (!sizeEc) {
        m_currentFileSize = static_cast<int>(fileSize);
    }
}

Logger::~Logger()
{
    if (m_logFile.is_open()) {
        m_logFile.flush();
        m_logFile.close();
    }
}

Logger& Logger::instance()
{
    static Logger s_instance;
    return s_instance;
}

void Logger::log(LogLevel level, const std::string &message, const LogData &data)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Rotate log file if necessary
    rotateLogFile();

    // Format log entry
    std::string logEntry = formatLogEntry(level, message, data);

    // Write to stderr
    fprintf(stderr, "%s\n", logEntry.c_str());

    // Write to file
    if (m_logFile.is_open()) {
        m_logFile << logEntry << "\n";
        m_logFile.flush();
        m_currentFileSize += static_cast<int>(logEntry.size()) + 1; // +1 for newline
    }
}

void Logger::rotateLogFile()
{
    if (m_currentFileSize >= m_maxFileSize) {
        m_logFile.flush();
        m_logFile.close();

        // Build timestamp string for rotated file name
        auto now = std::chrono::system_clock::now();
        std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
        std::tm tmBuf{};
#ifdef _WIN32
        localtime_s(&tmBuf, &nowTime);
#else
        localtime_r(&nowTime, &tmBuf);
#endif
        char timeBuf[64];
        std::strftime(timeBuf, sizeof(timeBuf), "%Y%m%d-%H%M%S", &tmBuf);

        std::string currentLogPath = "logs/app.log";
        std::string rotatedLogPath = currentLogPath + "." + timeBuf;

        std::error_code ec;
        std::filesystem::rename(currentLogPath, rotatedLogPath, ec);
        if (ec) {
            fprintf(stderr, "[WARN] Failed to rotate log file: %s\n", ec.message().c_str());
        }

        // Open new log file
        m_logFile.open(currentLogPath, std::ios::out | std::ios::app);
        if (!m_logFile.is_open()) {
            fprintf(stderr, "[WARN] Failed to open new log file: %s\n", currentLogPath.c_str());
            return;
        }
        m_currentFileSize = 0;
    }
}

std::string Logger::formatLogEntry(LogLevel level, const std::string &message, const LogData &data)
{
    // Build a simple JSON string manually (no Qt, no external JSON lib required)
    std::ostringstream ss;

    // Timestamp
    auto now = std::chrono::system_clock::now();
    std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
#ifdef _WIN32
    localtime_s(&tmBuf, &nowTime);
#else
    localtime_r(&nowTime, &tmBuf);
#endif
    char timeBuf[64];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%S", &tmBuf);

    // Level string
    const char* levelStr = "UNKNOWN";
    switch (level) {
    case Debug:   levelStr = "DEBUG";   break;
    case Info:    levelStr = "INFO";    break;
    case Warning: levelStr = "WARNING"; break;
    case Error:   levelStr = "ERROR";   break;
    }

    // Build JSON manually with minimal escaping
    ss << "{\"timestamp\":\"" << timeBuf
       << "\",\"level\":\"" << levelStr
       << "\",\"message\":\"";

    // Minimal JSON string escaping for message
    for (char c : message) {
        switch (c) {
        case '\"': ss << "\\\""; break;
        case '\\': ss << "\\\\"; break;
        case '\n': ss << "\\n";  break;
        case '\r': ss << "\\r";  break;
        case '\t': ss << "\\t";  break;
        default:   ss << c;      break;
        }
    }
    ss << "\"";

    // Append structured data fields
    for (const auto &[key, val] : data) {
        ss << ",\"" << key << "\":\"";
        for (char c : val) {
            switch (c) {
            case '\"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\n': ss << "\\n";  break;
            default:   ss << c;      break;
            }
        }
        ss << "\"";
    }

    ss << "}";
    return ss.str();
}
