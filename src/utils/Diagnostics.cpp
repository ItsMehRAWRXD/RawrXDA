#include "Diagnostics.hpp"

std::ofstream Diagnostics::s_logFile;
Diagnostics::LogLevel Diagnostics::s_minLevel = LogLevel::Info;
bool Diagnostics::s_initialized = false;

void Diagnostics::initialize(const std::string& logFilePath, LogLevel minLevel)
{
    if (s_initialized)
    {
        return;  // Already initialized
    }

    s_logFile.open(logFilePath, std::ios::app);
    s_minLevel = minLevel;
    s_initialized = true;

    if (s_logFile.is_open())
    {
        log(LogLevel::Info, "Diagnostics system initialized", "Diagnostics");
    }
}

void Diagnostics::shutdown()
{
    if (!s_initialized)
    {
        return;
    }

    log(LogLevel::Info, "Diagnostics system shutting down", "Diagnostics");

    if (s_logFile.is_open())
    {
        s_logFile.flush();
        s_logFile.close();
    }

    s_initialized = false;
}

void Diagnostics::log(LogLevel level, const std::string& message, const std::string& component)
{
    if (!s_initialized || level < s_minLevel)
    {
        return;  // Not initialized or below minimum level
    }

    if (!s_logFile.is_open())
    {
        return;  // Log file not available
    }

    std::ostringstream logEntry;
    logEntry << "[" << getCurrentTimestamp() << "] "
             << "[" << logLevelToString(level) << "] ";

    if (!component.empty())
    {
        logEntry << "[" << component << "] ";
    }

    logEntry << message << std::endl;

    s_logFile << logEntry.str();
    s_logFile.flush();  // Ensure immediate write for debugging
}

void Diagnostics::debug(const std::string& message, const std::string& component)
{
    log(LogLevel::Debug, message, component);
}

void Diagnostics::info(const std::string& message, const std::string& component)
{
    log(LogLevel::Info, message, component);
}

void Diagnostics::warning(const std::string& message, const std::string& component)
{
    log(LogLevel::Warning, message, component);
}

void Diagnostics::error(const std::string& message, const std::string& component)
{
    log(LogLevel::Error, message, component);
}

void Diagnostics::critical(const std::string& message, const std::string& component)
{
    log(LogLevel::Critical, message, component);
}

void Diagnostics::setMinLogLevel(LogLevel level)
{
    s_minLevel = level;
}

std::string Diagnostics::getCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif

    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}

std::string Diagnostics::logLevelToString(LogLevel level)
{
    switch (level)
    {
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO";
        case LogLevel::Warning:  return "WARNING";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
        default:                 return "UNKNOWN";
    }
}
