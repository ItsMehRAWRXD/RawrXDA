// ============================================================================
// logger.cpp — Logger Implementation (Structured JSON logging)
// ============================================================================
// Provides Logger::instance() and log() with structured JSON to file and
// human-readable line to stdout. Thread-safe singleton.
// ============================================================================

#include "telemetry/logger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdarg>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

// Escape string for JSON (quotes and backslashes)
static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (static_cast<unsigned char>(c) < 32) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
            out += buf;
        } else out += c;
    }
    return out;
}

// Return log file path: RAWRXD_LOG_FILE env, or %APPDATA%\RawrXD\ide.log (Win), or ./rawrxd.log
static std::string getLogFilePath() {
    const char* env = std::getenv("RAWRXD_LOG_FILE");
    if (env && env[0]) return std::string(env);
#ifdef _WIN32
    char path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        std::string dir = std::string(path) + "\\RawrXD";
        CreateDirectoryA(dir.c_str(), nullptr);
        return dir + "\\ide.log";
    }
#endif
    return "rawrxd.log";
}

Logger::Logger() {
    // Lazy-open log file on first log()
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

    // Human-readable to stdout (use formatLogEntry)
    std::string line = formatLogEntry(level, message, data);
    std::cout << line << std::endl;

    // Structured JSON to file (one line per entry)
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif
    char ts[32];
    snprintf(ts, sizeof(ts), "%04d-%02d-%02dT%02d:%02d:%02d.%03d",
             tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
             tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, (int)ms.count());
    const char* levelStr = "INFO";
    if (level == Debug) levelStr = "DEBUG";
    if (level == Warning) levelStr = "WARN";
    if (level == Error) levelStr = "ERROR";
    std::ostringstream json;
    json << "{\"ts\":\"" << ts << "\",\"level\":\"" << levelStr << "\",\"msg\":\"" << jsonEscape(message) << "\"";
    if (!data.empty()) {
        json << ",\"data\":{";
        bool first = true;
        for (const auto& [key, value] : data) {
            if (!first) json << ",";
            json << "\"" << jsonEscape(key) << "\":\"" << jsonEscape(value) << "\"";
            first = false;
        }
        json << "}";
    }
    json << "}\n";
    std::string jsonLine = json.str();

    if (!m_logFile.is_open()) {
        std::string path = getLogFilePath();
        m_logFile.open(path, std::ios::out | std::ios::app);
        m_currentFileSize = 0;
        if (m_logFile.is_open()) {
            m_logFile.seekp(0, std::ios::end);
            m_currentFileSize = static_cast<int>(m_logFile.tellp());
        }
    }
    if (m_logFile.is_open()) {
        m_logFile << jsonLine;
        m_logFile.flush();
        m_currentFileSize += static_cast<int>(jsonLine.size());
        if (m_currentFileSize >= m_maxFileSize) {
            rotateLogFile();
        }
    }
}

// ============================================================================
// RawrXD::Telemetry::Logger Implementation (Satisfy Linker)
// ============================================================================
#include "telemetry/UnifiedTelemetryCore.h"
#include <cstdarg>

namespace RawrXD::Telemetry {

class Logger {
public:
    static void Log(TelemetryLevel level, const char* fmt, ...) {
        char buffer[2048];
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
    std::string path = getLogFilePath();
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
    // Rotate: rename current to .1, reopen (keep one backup)
    std::string path1 = path + ".1";
#ifdef _WIN32
    MoveFileExA(path.c_str(), path1.c_str(), MOVEFILE_REPLACE_EXISTING);
#else
    rename(path.c_str(), path1.c_str());
#endif
    m_logFile.open(path, std::ios::out | std::ios::app);
    m_currentFileSize = m_logFile.is_open() ? static_cast<int>(m_logFile.tellp()) : 0;
}
