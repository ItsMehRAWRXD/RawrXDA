#pragma once

#include <string>
#include <fstream>
#include <chrono>
#include <sstream>
#include <iomanip>

/**
 * @brief Structured logging and diagnostics utility.
 *
 * Provides hierarchical logging levels, automatic timestamps, and file-based
 * output for debugging and monitoring. All diagnostic output should funnel
 * through this class to maintain consistency.
 */
class Diagnostics
{
public:
    enum class LogLevel
    {
        Debug,
        Info,
        Warning,
        Error,
        Critical
    };

    /**
     * Initialize the diagnostics system.
     * @param logFilePath Path to the log file (default: RawrXD_diagnostics.log)
     * @param minLevel Minimum log level to record (default: Info)
     */
    static void initialize(const std::string& logFilePath = "RawrXD_diagnostics.log",
                          LogLevel minLevel = LogLevel::Info);

    /**
     * Shutdown the diagnostics system and flush logs.
     */
    static void shutdown();

    /**
     * Log a message at the specified level.
     * @param level Log level
     * @param message Message to log
     * @param component Optional component name (e.g., "ModelLoader", "InferenceEngine")
     */
    static void log(LogLevel level, const std::string& message,
                   const std::string& component = "");

    // Convenience methods for each log level
    static void debug(const std::string& message, const std::string& component = "");
    static void info(const std::string& message, const std::string& component = "");
    static void warning(const std::string& message, const std::string& component = "");
    static void error(const std::string& message, const std::string& component = "");
    static void critical(const std::string& message, const std::string& component = "");

    /**
     * Set the minimum log level filter.
     */
    static void setMinLogLevel(LogLevel level);

private:
    static std::ofstream s_logFile;
    static LogLevel s_minLevel;
    static bool s_initialized;

    static std::string getCurrentTimestamp();
    static std::string logLevelToString(LogLevel level);
};
