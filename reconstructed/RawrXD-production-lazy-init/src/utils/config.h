#pragma once

#include <string>
#include <cstdlib>
#include <sstream>

namespace RawrXD {
namespace Utils {

inline std::string getenv_or(const std::string& name, const std::string& fallback) {
    const char* v = std::getenv(name.c_str());
    if (!v) return fallback;
    return std::string(v);
}

inline int getenv_or_int(const std::string& name, int fallback) {
    const char* v = std::getenv(name.c_str());
    if (!v) return fallback;
    try {
        return std::stoi(std::string(v));
    } catch (...) {
        return fallback;
    }
}

} // namespace Utils
} // namespace RawrXD
