#pragma once
#include <string>
#include <iostream>

class Logger {
public:
    virtual ~Logger() = default;
    virtual void log(const std::string& message) {
        std::cout << "[LOG] " << message << std::endl;
    }
    virtual void error(const std::string& message) {
        std::cerr << "[ERROR] " << message << std::endl;
    }
};
