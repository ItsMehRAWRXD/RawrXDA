#pragma once
#include <string>
#include <iostream>

class Logger {
public:
    virtual ~Logger() = default;
    virtual void log(const std::string& message);
    virtual void error(const std::string& message);
    virtual void warn(const std::string& message);
    virtual void info(const std::string& message);
    virtual void debug(const std::string& message);
    static bool initializeFile(const std::string& path);
    static void shutdown();
};
