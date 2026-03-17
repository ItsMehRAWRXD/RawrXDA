#pragma once

#include <string>

namespace Logger {
    void info(const std::string& msg);
    void warn(const std::string& msg);
    void error(const std::string& msg);
}
