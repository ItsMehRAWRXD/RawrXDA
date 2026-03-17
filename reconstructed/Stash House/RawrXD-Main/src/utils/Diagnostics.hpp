#pragma once

#include <fstream>
#include <string>

class Diagnostics {
public:
    enum class LogLevel {
        Debug,
        Info,
        Warning,
        Error,
        Critical
    };

    static void initialize(const std::string& logFilePath, LogLevel minLevel = LogLevel::Info);
    static void shutdown();

    static void log(LogLevel level, const std::string& message, const std::string& component = "");
    static void debug(const std::string& message, const std::string& component = "");
    static void info(const std::string& message, const std::string& component = "");
    static void warning(const std::string& message, const std::string& component = "");
    static void error(const std::string& message, const std::string& component = "");
    static void critical(const std::string& message, const std::string& component = "");

    static void setMinLogLevel(LogLevel level);

    static std::string getCurrentTimestamp();
    static std::string logLevelToString(LogLevel level);

private:
    static std::ofstream s_logFile;
    static LogLevel s_minLevel;
    static bool s_initialized;
};
