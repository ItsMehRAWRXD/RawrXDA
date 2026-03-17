// ============================================================================
// File: src/common/string_utils.hpp
// Purpose: String utilities replacing QString operations
// No Qt dependency - pure C++17
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <regex>

namespace StringUtils {

inline bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

inline bool containsCI(const std::string& haystack, const std::string& needle) {
    std::string h = haystack, n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}

inline bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

inline bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && 
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline std::string trimmed(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

inline std::string toLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

inline std::string toUpper(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

inline std::vector<std::string> split(const std::string& s, char delimiter, bool skipEmpty = false) {
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, delimiter)) {
        if (!skipEmpty || !token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

inline std::vector<std::string> split(const std::string& s, const std::string& delimiter, bool skipEmpty = false) {
    std::vector<std::string> tokens;
    size_t pos = 0, prev = 0;
    while ((pos = s.find(delimiter, prev)) != std::string::npos) {
        std::string token = s.substr(prev, pos - prev);
        if (!skipEmpty || !token.empty()) {
            tokens.push_back(token);
        }
        prev = pos + delimiter.size();
    }
    std::string last = s.substr(prev);
    if (!skipEmpty || !last.empty()) {
        tokens.push_back(last);
    }
    return tokens;
}

inline std::string join(const std::vector<std::string>& parts, const std::string& sep) {
    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += sep;
        result += parts[i];
    }
    return result;
}

inline std::string replace(const std::string& s, const std::string& from, const std::string& to) {
    std::string result = s;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.size(), to);
        pos += to.size();
    }
    return result;
}

inline int count(const std::string& s, const std::string& sub) {
    int cnt = 0;
    size_t pos = 0;
    while ((pos = s.find(sub, pos)) != std::string::npos) {
        ++cnt;
        pos += sub.size();
    }
    return cnt;
}

inline std::string mid(const std::string& s, size_t pos, size_t len = std::string::npos) {
    if (pos >= s.size()) return "";
    return s.substr(pos, len);
}

inline bool isEmpty(const std::string& s) {
    return s.empty();
}

inline std::string number(int val) {
    return std::to_string(val);
}

inline std::string number(double val) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.6g", val);
    return buf;
}

} // namespace StringUtils
