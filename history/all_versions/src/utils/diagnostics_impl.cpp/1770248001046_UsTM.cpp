#include "../utils/Diagnostics.hpp"
#include <iostream>

namespace Diagnostics {

void error(const std::string& component, const std::string& message) {
    std::cerr << "[ERROR] " << component << ": " << message << std::endl;
}

void warn(const std::string& component, const std::string& message) {
    std::cerr << "[WARN] " << component << ": " << message << std::endl;
}

void info(const std::string& component, const std::string& message) {
    std::cout << "[INFO] " << component << ": " << message << std::endl;
}

} // namespace Diagnostics
