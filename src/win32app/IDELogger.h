#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <spdlog/spdlog.h>

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERR, // Renamed to avoid macro conflict
    CRITICAL
};

class IDELogger {
public:
    static IDELogger& getInstance();
    void log(LogLevel level, const std::string& message);

private:
    IDELogger() = default;
};
