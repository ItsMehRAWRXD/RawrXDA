#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <map>
#include <mutex>
#include <fstream>
#include <iostream>

// Structured JSON logs → stdout + rotating file
class Logger
{
public:
    enum LogLevel {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3
    };

    using LogData = std::map<std::string, std::string>;

    Logger();
    ~Logger();

    // Log a message with structured data
    void log(LogLevel level, const std::string &message, const LogData &data = {});

    // Convenience functions
    void logDebug(const std::string &message, const LogData &data = {}) {
        log(Debug, message, data);
    }
    void logInfo(const std::string &message, const LogData &data = {}) {
        log(Info, message, data);
    }
    void logWarning(const std::string &message, const LogData &data = {}) {
        log(Warning, message, data);
    }
    void logError(const std::string &message, const LogData &data = {}) {
        log(Error, message, data);
    }
    
    static Logger& instance();

private:
    std::mutex m_mutex;
    std::ofstream m_logFile;
    int m_maxFileSize = 10 * 1024 * 1024; // 10 MB
    int m_currentFileSize = 0;

    void rotateLogFile();
    std::string formatLogEntry(LogLevel level, const std::string &message, const LogData &data);
};

#endif // LOGGER_H
