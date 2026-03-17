#pragma once
#include <string>
#include <iostream>
#include <sstream>

class Logger {
    std::string m_prefix;
public:
    Logger() : m_prefix("LOG") {}
    explicit Logger(const std::string& prefix) : m_prefix(prefix) {}
    virtual ~Logger() = default;

    virtual void log(const std::string& message) {
        std::cout << "[" << m_prefix << "] " << message << std::endl;
    }
    virtual void info(const std::string& message) {
        std::cout << "[" << m_prefix << "] " << message << std::endl;
    }
    virtual void error(const std::string& message) {
        std::cerr << "[" << m_prefix << " ERROR] " << message << std::endl;
    }
    virtual void warn(const std::string& message) {
        std::cerr << "[" << m_prefix << " WARN] " << message << std::endl;
    }
};
