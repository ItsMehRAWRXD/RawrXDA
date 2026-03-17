#include "../utils/Diagnostics.hpp"
#include <iostream>

void Diagnostics::error(const std::string& message, const std::string& component) {
    std::cerr << "[ERROR] " << (component.empty() ? "" : component + ": ") << message << std::endl;
}

void Diagnostics::warning(const std::string& message, const std::string& component) {
    std::cerr << "[WARN] " << (component.empty() ? "" : component + ": ") << message << std::endl;
}

void Diagnostics::info(const std::string& message, const std::string& component) {
    std::cout << "[INFO] " << (component.empty() ? "" : component + ": ") << message << std::endl;
}

void Diagnostics::debug(const std::string& message, const std::string& component) {
    #ifdef _DEBUG
    std::cout << "[DEBUG] " << (component.empty() ? "" : component + ": ") << message << std::endl;
    #endif
}

void Diagnostics::critical(const std::string& message, const std::string& component) {
    std::cerr << "[CRITICAL] " << (component.empty() ? "" : component + ": ") << message << std::endl;
}

void Diagnostics::initialize(const std::string& logFilePath, LogLevel minLevel) {
    // Basic init
}

void Diagnostics::shutdown() {
    // Basic shutdown
}

void Diagnostics::log(LogLevel level, const std::string& message, const std::string& component) {
    switch (level) {
        case LogLevel::Info: info(message, component); break;
        case LogLevel::Warning: warning(message, component); break;
        case LogLevel::Error: error(message, component); break;
        case LogLevel::Critical: critical(message, component); break;
        case LogLevel::Debug: debug(message, component); break;
    }
}
