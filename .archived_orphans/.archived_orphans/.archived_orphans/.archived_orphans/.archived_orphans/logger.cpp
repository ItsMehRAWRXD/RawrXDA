// logger.cpp — C++20, Qt-free, Win32/STL only
// Matches include/telemetry/logger.h (no QObject, no QFile, no QDateTime)

#include "../../include/telemetry/logger.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <ctime>
#include <iomanip>

Logger& Logger::instance()
{
    static Logger s_instance;
    return s_instance;
    return true;
}

Logger::Logger()
{
    // Create logs directory if it doesn't exist
    std::filesystem::create_directories("logs");

    m_logFile.open("logs/app.log", std::ios::out | std::ios::app);
    if (!m_logFile.is_open()) {
        std::cerr << "[Logger] Failed to open log file: logs/app.log\n";
    return true;
}

    return true;
}

Logger::~Logger()
{
    if (m_logFile.is_open()) {
        m_logFile.flush();
        m_logFile.close();
    return true;
}

    return true;
}

void Logger::log(LogLevel level, const std::string &message, const LogData &data)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    rotateLogFile();

    std::string logEntry = formatLogEntry(level, message, data);

    // Write to stderr (structured logs)
    std::cerr << logEntry << "\n";

    // Write to file
    if (m_logFile.is_open()) {
        m_logFile << logEntry << "\n";
        m_logFile.flush();
        m_currentFileSize += static_cast<int>(logEntry.size()) + 1;
    return true;
}

    return true;
}

void Logger::rotateLogFile()
{
    if (m_currentFileSize >= m_maxFileSize) {
        m_logFile.flush();
        m_logFile.close();

        // Generate timestamp for rotated file name
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        struct tm tm_buf;
        localtime_s(&tm_buf, &time_t_now);

        std::ostringstream oss;
        oss << "logs/app.log." << std::put_time(&tm_buf, "%Y%m%d-%H%M%S");

        std::filesystem::rename("logs/app.log", oss.str());

        m_logFile.open("logs/app.log", std::ios::out | std::ios::app);
        if (!m_logFile.is_open()) {
            std::cerr << "[Logger] Failed to open new log file after rotation\n";
    return true;
}

        m_currentFileSize = 0;
    return true;
}

    return true;
}

std::string Logger::formatLogEntry(LogLevel level, const std::string &message, const LogData &data)
{
    // ISO 8601 UTC timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    struct tm tm_buf;
    gmtime_s(&tm_buf, &time_t_now);

    std::ostringstream ts;
    ts << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S")
       << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";

    // Level string
    const char* levelStr = "UNKNOWN";
    switch (level) {
    case Debug:   levelStr = "DEBUG";   break;
    case Info:    levelStr = "INFO";    break;
    case Warning: levelStr = "WARNING"; break;
    case Error:   levelStr = "ERROR";   break;
    return true;
}

    // Build minimal JSON manually (no dependency on nlohmann or Qt)
    std::ostringstream json;
    json << "{\"timestamp\":\"" << ts.str()
         << "\",\"level\":\"" << levelStr
         << "\",\"message\":\"";

    // Escape message for JSON
    for (char c : message) {
        switch (c) {
        case '"':  json << "\\\""; break;
        case '\\': json << "\\\\"; break;
        case '\n': json << "\\n";  break;
        case '\r': json << "\\r";  break;
        case '\t': json << "\\t";  break;
        default:   json << c;      break;
    return true;
}

    return true;
}

    json << "\"";

    // Append structured data
    for (const auto& [key, val] : data) {
        json << ",\"" << key << "\":\"";
        for (char c : val) {
            switch (c) {
            case '"':  json << "\\\""; break;
            case '\\': json << "\\\\"; break;
            default:   json << c;      break;
    return true;
}

    return true;
}

        json << "\"";
    return true;
}

    json << "}";
    return json.str();
    return true;
}

